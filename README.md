# DCN Assignment 2 — 数据库与业务逻辑模块说明（成员 2）

本仓库/目录中由 **成员 2** 负责的部分，对应作业 *Course Timetable Inquiry System* 里的 **Database Module** 与 **User Management（管理员账号校验）** 的后端实现。其它同学（尤其是 **成员 1：网络与并发 / 服务端主程序**）只需要调用 **`handleRequest`**，不必直接读写 CSV 或解析业务规则。

---

## 我负责做什么

- 用 **CSV 文件**（`courses.csv`）持久化课表：**课程代码、标题、班级、教师、星期、时间段、教室、学期**。
- 用 **文本文件**（`users.txt`）保存 **管理员账号**（用户名 + 密码，作业要求的简单认证）。
- 实现作业要求的查询与维护能力：
  - 按 **课程代码** 查询  
  - 按 **教师** 查询（名字 **子串**、**不区分大小写**）  
  - **查看全部**课表，可选按 **学期** 过滤（例如 `2026S2`）  
  - 管理员：**添加 / 修改 / 删除** 课程
- 将以上能力 **封装为一个函数 `handleRequest`**，供成员 1 在收到客户端报文后调用。
- 使用 **`std::mutex`** 保护文件读写与内存处理，避免多线程并发时数据损坏（满足作业并发场景）。

---

## 交付文件一览

| 文件 | 说明 |
|------|------|
| `database.h` | 声明 `ClientSession`、`handleRequest`、`setDatabaseDataDirectory` |
| `database.cpp` | 实现逻辑与 CSV/用户文件读写 |
| `courses.csv` | 课表数据（当前 **502** 条课程记录：500 条批量示例 + 作业示例 `COMP3003` A/B 共 2 条） |
| `users.txt` | 管理员列表（`#` 开头为注释行） |
| `README.md` | 本说明，给全组同学阅读 |
| `timetable_gui.cpp` | **本地测试用** Windows 图形界面：按钮与输入框组装协议文本，内部仍调用 `handleRequest`（与网络版共用逻辑） |
| `build_gui.bat` | MinGW 下一键编译 `timetable_gui.exe` |

---

## 图形界面（本地客户端 / 作业 Bonus）

`Timetable client (local GUI)` 是一个 **Win32** 小窗口，方便在成员 1 的 Winsock 程序未完成前 **调试数据与协议**；也符合作业里 *GUI 客户端* 的加分方向说明。

- **不替代**成员 1 的 socket 客户端；它只是在本机直接调用 `handleRequest`。
- 启动时会把 **数据目录设为 exe 所在文件夹**（`setDatabaseDataDirectory`），因此请把 **`courses.csv`、`users.txt` 与 `timetable_gui.exe` 放在同一目录**，或在源码里改路径。
- 界面包含：按代码/教师/学期查询、管理员登录、添加（单行 CSV）、更新、删除、以及 **原始协议**输入框。

### 编译（MinGW）

在项目目录双击或命令行运行：

```bat
build_gui.bat
```

或手动：

```bat
g++ -std=c++17 -O2 timetable_gui.cpp database.cpp -o timetable_gui.exe -mwindows -municode -luser32 -lgdi32 -lcomctl32
```

注意链接选项 **`-municode`**：程序入口为 `wWinMain`（Unicode 窗口）。

### Visual Studio

将 `timetable_gui.cpp` 与 `database.cpp` 加入工程，子系统选 **Windows**，字符集选 **Unicode**，同样链接 `user32` / `gdi32` / `comctl32`。

---

## 成员 1 如何调用（唯一需要对接的接口）

### 1. 每个客户端连接一个会话对象

```cpp
#include "database.h"

ClientSession session;  // 每个 TCP 连接各有一个，不要多连接共用
```

管理员登录成功后，`session.isAdmin` 会为 `true`；普通学生不必登录即可查询（由成员 1 决定是否在客户端限制命令）。

### 2. 每收到一行请求，调用一次

```cpp
std::string line = /* 从 socket 读入的一行，去掉末尾 \r\n */;
std::string reply = handleRequest(line, session);
/* 将 reply 发回客户端；若 reply 内含换行，需按多行原样发送 */
```

### 3. 工作目录与数据路径

默认在当前工作目录查找 `courses.csv` 和 `users.txt`。若成员 1 的可执行文件运行目录不同，可在启动时调用一次：

```cpp
setDatabaseDataDirectory("F:\\学习\\Year_2_semester_2\\dcn\\PROJECT");
```

（路径按实际部署修改；末尾反斜杠可有可无。）

---

## 文本协议（与 `handleRequest` 约定）

以下均为 **单行命令**（字符串里不要再带未转义的换行）。响应可能是 **多行**，用 `\n` 分隔。

### 通用

| 请求 | 响应说明 |
|------|----------|
| `LOGIN <用户名> <密码>` | `SUCCESS` 或 `FAILURE`（密码中请勿含空格，与 `users.txt` 格式一致） |
| `LOGOUT` | `OK` |

### 查询（学生/管理员均可，由成员 1 在客户端控制）

| 请求 | 响应说明 |
|------|----------|
| `QUERY CODE <课程代码>` | 无结果：`RESULT NOT FOUND`；有结果：首行 `RESULT <数量>`，后续每行一门课（人类可读摘要） |
| `QUERY INSTRUCTOR <教师名>` | 同上（子串匹配） |
| `QUERY ALL` | 全部课程 |
| `QUERY ALL <学期>` | 仅该学期，例如 `QUERY ALL 2026S2` |

### 管理员（需已成功 `LOGIN`）

| 请求 | 响应说明 |
|------|----------|
| `ADD COURSE <8个字段的CSV，逗号分隔>` | 成功 `OK`；失败以 `ERROR` 开头。字段顺序与表头一致：`code,title,section,instructor,day,time,classroom,semester` |
| `UPDATE <代码> [SECTION <班级>] <字段> <新值...>` | 例：`UPDATE COMP3003 TIME Mon-10:00`。字段名：`TITLE` `SECTION` `INSTRUCTOR` `DAY` `TIME` `CLASSROOM` `SEMESTER` `CODE` |
| `DELETE <代码> [SECTION <班级>]` | 删除匹配的一条；多 section 时建议写 `SECTION` |

未登录的管理员写操作会返回 `ERROR unauthorized`。

更完整的注释见 `database.h` 文件顶部。

---

## 数据文件格式

### `courses.csv`

- 第一行为表头：  
  `code,title,section,instructor,day,time,classroom,semester`
- 每一行一门课；若字段内含逗号，实现上支持 CSV 双引号转义（建议新数据尽量不用裸逗号，便于人工检查）。
- 当前数据量为 **大批量随机生成的示例课表**（用于测试查询性能与并发），并 **追加** 了与作业描述一致的 `COMP3003` A/B 班记录，便于演示。

### `users.txt`

- 每行：`用户名 密码`（空格分隔）
- `#` 开头为注释

默认示例：`admin` / `admin123`（提交前请全组修改为更安全的口令，或按老师要求处理）。

---

## 编译提示

- 使用 **C++17** 或以上（使用了标准库线程互斥等）。
- 将 `database.cpp` 与成员 1 的工程一起编译链接即可；本模块 **不依赖 Winsock**，网络代码全部在成员 1 侧。

---

## 联系与协作建议

- **成员 1**：实现 socket、并发、把客户端字符串 **原样**（或统一 trim 后）交给 `handleRequest`，并把返回字符串写回 socket。  
- **成员 3/4**：若负责客户端或报告，可直接引用本文 **协议表** 作为「应用层协议」说明。  
- 若全组希望改用 **与 PDF 示例完全一致** 的另一种拼写（例如统一大写、关键字命名），可在定稿后由成员 2 **只改 `database.cpp` 解析部分**，接口仍保持 `handleRequest` 不变。

如有协议变更，请先在组内同步，再改 `database.h` 顶部注释与本文档。
