// Local test client with Win32 GUI. Calls the same handleRequest() as the network server will.
// Build (MinGW): g++ -std=c++17 -O2 timetable_gui.cpp database.cpp -o timetable_gui.exe -mwindows -municode -luser32 -lgdi32 -lcomctl32

#include "database.h"

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <commctrl.h>

#include <string>
#include <vector>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")

namespace {

constexpr int kPad = 10;
constexpr int kLine = 26;

enum : int {
    IDC_OUTPUT = 2000,
    IDC_BTN_CLEAR,
    IDC_LBL_CODE,
    IDC_ED_CODE,
    IDC_BTN_QCODE,
    IDC_LBL_INST,
    IDC_ED_INST,
    IDC_BTN_QINST,
    IDC_LBL_SEM,
    IDC_ED_SEM,
    IDC_BTN_QALL,
    IDC_LBL_USER,
    IDC_ED_USER,
    IDC_LBL_PASS,
    IDC_ED_PASS,
    IDC_BTN_LOGIN,
    IDC_BTN_LOGOUT,
    IDC_LBL_ADD,
    IDC_ED_ADD,
    IDC_BTN_ADD,
    IDC_LBL_UP,
    IDC_ED_UPCODE,
    IDC_ED_UPSEC,
    IDC_ED_FIELD,
    IDC_ED_UPVAL,
    IDC_BTN_UPDATE,
    IDC_LBL_DEL,
    IDC_ED_DELCODE,
    IDC_ED_DELSEC,
    IDC_BTN_DELETE,
    IDC_LBL_RAW,
    IDC_ED_RAW,
    IDC_BTN_SENDRAW,
    IDC_STATUS,
};

HWND g_hwndMain = nullptr;
HWND g_hwndOutput = nullptr;
HWND g_hwndStatus = nullptr;
HWND g_hwndClear = nullptr;
int g_logTop = 0;
ClientSession g_session;

std::string wideToUtf8(const std::wstring& w) {
    if (w.empty()) {
        return {};
    }
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) {
        return {};
    }
    std::string s(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), s.data(), n, nullptr, nullptr);
    return s;
}

std::wstring utf8ToWide(const std::string& u8) {
    if (u8.empty()) {
        return {};
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), static_cast<int>(u8.size()), nullptr, 0);
    if (n <= 0) {
        return {};
    }
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, u8.c_str(), static_cast<int>(u8.size()), w.data(), n);
    return w;
}

std::wstring getEditText(HWND h) {
    int len = GetWindowTextLengthW(h);
    if (len <= 0) {
        return {};
    }
    std::wstring w(static_cast<std::size_t>(len) + 1, L'\0');
    GetWindowTextW(h, w.data(), len + 1);
    w.resize(static_cast<std::size_t>(len));
    return w;
}

void appendOutputW(const std::wstring& text) {
    if (!g_hwndOutput) {
        return;
    }
    SendMessageW(g_hwndOutput, EM_SETSEL, static_cast<WPARAM>(-1), static_cast<LPARAM>(-1));
    SendMessageW(g_hwndOutput, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(text.c_str()));
    SendMessageW(g_hwndOutput, EM_SCROLLCARET, 0, 0);
}

void appendOutputUtf8(const std::string& u8) {
    appendOutputW(utf8ToWide(u8));
}

void updateStatus() {
    if (!g_hwndStatus) {
        return;
    }
    std::wstring s;
    if (g_session.isAdmin) {
        s = L"Role: Administrator (";
        s += utf8ToWide(g_session.username);
        s += L")";
    } else {
        s = L"Role: guest (student, query only). Log in as administrator to add, update, or delete courses.";
    }
    SetWindowTextW(g_hwndStatus, s.c_str());
}

void runLine(const std::string& line) {
    std::string reply = handleRequest(line, g_session);
    appendOutputUtf8("> ");
    appendOutputUtf8(line);
    appendOutputUtf8("\r\n");
    appendOutputUtf8(reply);
    appendOutputUtf8("\r\n\r\n");
    updateStatus();
}

void stripExeName(wchar_t* path) {
    wchar_t* slash = wcsrchr(path, L'\\');
    if (!slash) {
        slash = wcsrchr(path, L'/');
    }
    if (slash) {
        *slash = L'\0';
    }
}

void initDataDirectoryNearExe() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        return;
    }
    stripExeName(buf);
    std::string dir = wideToUtf8(buf);
    if (!dir.empty()) {
        setDatabaseDataDirectory(dir);
    }
}

HWND makeLabel(HWND parent, int id, int x, int y, int w, int h, const wchar_t* text) {
    return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                           nullptr, nullptr);
}

HWND makeEdit(HWND parent, int id, int x, int y, int w, int h, bool multiline = false) {
    DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL;
    if (multiline) {
        style |= ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_WANTRETURN;
    }
    return CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", style, x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                           nullptr, nullptr);
}

HWND makeBtn(HWND parent, int id, int x, int y, int w, int h, const wchar_t* text) {
    return CreateWindowExW(0, L"BUTTON", text, WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, x, y, w, h, parent,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
}

void resizeLogPane(int clientW, int clientH) {
    if (!g_hwndOutput || g_logTop <= 0) {
        return;
    }
    const int bottomBar = 36;
    int fullW = clientW - 2 * kPad;
    int h = clientH - g_logTop - bottomBar;
    if (h < 80) {
        h = 80;
    }
    MoveWindow(g_hwndOutput, kPad, g_logTop, fullW, h, TRUE);
    if (g_hwndClear) {
        MoveWindow(g_hwndClear, kPad, clientH - bottomBar + 2, 130, 28, TRUE);
    }
}

void createControls(HWND parent, int clientW, int clientH) {
    int y = kPad;
    int fullW = clientW - 2 * kPad;
    int labW = 155;
    int btnW = 130;
    int edW = fullW - labW - btnW - 2 * kPad;

    g_hwndStatus = makeLabel(parent, IDC_STATUS, kPad, y, fullW, 20, L"");
    y += 28;

    makeLabel(parent, IDC_LBL_CODE, kPad, y, labW, kLine, L"Course code");
    HWND edCode = makeEdit(parent, IDC_ED_CODE, kPad + labW, y - 2, edW, kLine + 6);
    makeBtn(parent, IDC_BTN_QCODE, kPad + labW + edW + kPad, y - 2, btnW, kLine + 6, L"Query by code");
    y += kLine + kPad;

    makeLabel(parent, IDC_LBL_INST, kPad, y, labW, kLine, L"Instructor");
    makeEdit(parent, IDC_ED_INST, kPad + labW, y - 2, edW, kLine + 6);
    makeBtn(parent, IDC_BTN_QINST, kPad + labW + edW + kPad, y - 2, btnW, kLine + 6, L"Query by name");
    y += kLine + kPad;

    makeLabel(parent, IDC_LBL_SEM, kPad, y, labW, kLine, L"Semester (optional)");
    makeEdit(parent, IDC_ED_SEM, kPad + labW, y - 2, edW / 2, kLine + 6);
    makeBtn(parent, IDC_BTN_QALL, kPad + labW + edW + kPad, y - 2, btnW, kLine + 6, L"List all courses");
    y += kLine + kPad + 6;

    makeLabel(parent, 2098, kPad, y, fullW, 18, L"--- Administrator ---");
    y += 22;

    int half = (fullW - kPad) / 2;
    makeLabel(parent, IDC_LBL_USER, kPad, y, 70, kLine, L"Username");
    makeEdit(parent, IDC_ED_USER, kPad + 70, y - 2, half - 80, kLine + 6);
    makeLabel(parent, IDC_LBL_PASS, kPad + half, y, 70, kLine, L"Password");
    makeEdit(parent, IDC_ED_PASS, kPad + half + 70, y - 2, half - 80 - btnW - kPad, kLine + 6, false);
    HWND hPass = GetDlgItem(parent, IDC_ED_PASS);
    SendMessageW(hPass, EM_SETPASSWORDCHAR, static_cast<WPARAM>(L'*'), 0);
    makeBtn(parent, IDC_BTN_LOGIN, clientW - kPad - btnW, y - 2, btnW / 2 - 4, kLine + 6, L"Login");
    makeBtn(parent, IDC_BTN_LOGOUT, clientW - kPad - btnW / 2 + 4, y - 2, btnW / 2 - 4, kLine + 6, L"Logout");
    y += kLine + kPad;

    makeLabel(parent, IDC_LBL_ADD, kPad, y, fullW, 18, L"Add course (one CSV line, 8 fields)");
    y += 20;
    makeEdit(parent, IDC_ED_ADD, kPad, y, fullW - btnW - kPad, 52, true);
    makeBtn(parent, IDC_BTN_ADD, kPad + fullW - btnW, y, btnW, 52, L"Add");
    y += 58;

    makeLabel(parent, IDC_LBL_UP, kPad, y, fullW, 18, L"Update (admin): code / section / FIELD / new value");
    y += 20;
    {
        int wCode = 140;
        int wSec = 70;
        int wField = 110;
        int x0 = kPad;
        makeEdit(parent, IDC_ED_UPCODE, x0, y - 2, wCode, kLine + 6);
        int x1 = x0 + wCode + kPad;
        makeEdit(parent, IDC_ED_UPSEC, x1, y - 2, wSec, kLine + 6);
        int x2 = x1 + wSec + kPad;
        makeEdit(parent, IDC_ED_FIELD, x2, y - 2, wField, kLine + 6);
        int x3 = x2 + wField + kPad;
        int valW = clientW - x3 - btnW - 2 * kPad;
        if (valW < 120) {
            valW = 120;
        }
        makeEdit(parent, IDC_ED_UPVAL, x3, y - 2, valW, kLine + 6);
        makeBtn(parent, IDC_BTN_UPDATE, clientW - kPad - btnW, y - 2, btnW, kLine + 6, L"Update");
    }
    y += kLine + kPad;

    makeLabel(parent, IDC_LBL_DEL, kPad, y, fullW, 18, L"Delete: code | section (optional)");
    y += 20;
    int dq = (fullW - btnW - 2 * kPad) / 2;
    makeEdit(parent, IDC_ED_DELCODE, kPad, y, dq, kLine + 6);
    makeEdit(parent, IDC_ED_DELSEC, kPad + dq + kPad, y, dq, kLine + 6);
    makeBtn(parent, IDC_BTN_DELETE, clientW - kPad - btnW, y, btnW, kLine + 6, L"Delete");
    y += kLine + kPad + 6;

    makeLabel(parent, IDC_LBL_RAW, kPad, y, fullW, 18, L"Raw protocol line (advanced)");
    y += 20;
    makeEdit(parent, IDC_ED_RAW, kPad, y, fullW - btnW - kPad, kLine + 6);
    makeBtn(parent, IDC_BTN_SENDRAW, clientW - kPad - btnW, y, btnW, kLine + 6, L"Send");
    y += kLine + kPad + 8;

    g_logTop = y;
    g_hwndOutput = makeEdit(parent, IDC_OUTPUT, kPad, y, fullW, 120, true);
    SendMessageW(g_hwndOutput, EM_SETREADONLY, TRUE, 0);
    SendMessageW(g_hwndOutput, EM_SETLIMITTEXT, 0, 0x7FFFFFFFL);

    g_hwndClear = makeBtn(parent, IDC_BTN_CLEAR, kPad, clientH - 34, 130, 28, L"Clear log");

    SetWindowTextW(edCode, L"COMP3003");
    SetWindowTextW(GetDlgItem(parent, IDC_ED_SEM), L"2026S2");
    SetWindowTextW(GetDlgItem(parent, IDC_ED_USER), L"admin");
    SetWindowTextW(GetDlgItem(parent, IDC_ED_PASS), L"admin123");
    SetWindowTextW(GetDlgItem(parent, IDC_ED_FIELD), L"TIME");
    SetWindowTextW(GetDlgItem(parent, IDC_ED_UPVAL), L"Mon-10:00");

    HFONT f = reinterpret_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    HWND ch = GetWindow(parent, GW_CHILD);
    while (ch) {
        SendMessageW(ch, WM_SETFONT, reinterpret_cast<WPARAM>(f), TRUE);
        ch = GetWindow(ch, GW_HWNDNEXT);
    }

    updateStatus();
    resizeLogPane(clientW, clientH);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        g_hwndMain = hwnd;
        RECT rc{};
        GetClientRect(hwnd, &rc);
        createControls(hwnd, rc.right - rc.left, rc.bottom - rc.top);
        appendOutputW(
            L"Timetable local test client: calls handleRequest() and loads courses.csv / users.txt from the same folder as this .exe.\r\n\r\n");
        return 0;
    }
    case WM_SIZE: {
        int cw = LOWORD(lParam);
        int ch = HIWORD(lParam);
        if (cw > 0 && ch > 0) {
            resizeLogPane(cw, ch);
        }
        return 0;
    }
    case WM_COMMAND: {
        int id = LOWORD(wParam);
        if (id == IDC_BTN_CLEAR) {
            SetWindowTextW(g_hwndOutput, L"");
            return 0;
        }
        if (id == IDC_BTN_QCODE) {
            std::wstring w = getEditText(GetDlgItem(hwnd, IDC_ED_CODE));
            std::string u8 = wideToUtf8(w);
            if (u8.empty()) {
                appendOutputW(L"(empty course code)\r\n");
                return 0;
            }
            runLine("QUERY CODE " + u8);
            return 0;
        }
        if (id == IDC_BTN_QINST) {
            std::wstring w = getEditText(GetDlgItem(hwnd, IDC_ED_INST));
            std::string u8 = wideToUtf8(w);
            if (u8.empty()) {
                appendOutputW(L"(empty instructor)\r\n");
                return 0;
            }
            runLine("QUERY INSTRUCTOR " + u8);
            return 0;
        }
        if (id == IDC_BTN_QALL) {
            std::wstring sem = getEditText(GetDlgItem(hwnd, IDC_ED_SEM));
            std::string s = wideToUtf8(sem);
            if (s.empty()) {
                runLine("QUERY ALL");
            } else {
                runLine("QUERY ALL " + s);
            }
            return 0;
        }
        if (id == IDC_BTN_LOGIN) {
            std::string u = wideToUtf8(getEditText(GetDlgItem(hwnd, IDC_ED_USER)));
            std::string p = wideToUtf8(getEditText(GetDlgItem(hwnd, IDC_ED_PASS)));
            if (u.empty() || p.empty()) {
                appendOutputW(L"(need username and password)\r\n");
                return 0;
            }
            runLine("LOGIN " + u + " " + p);
            return 0;
        }
        if (id == IDC_BTN_LOGOUT) {
            runLine("LOGOUT");
            return 0;
        }
        if (id == IDC_BTN_ADD) {
            std::string csv = wideToUtf8(getEditText(GetDlgItem(hwnd, IDC_ED_ADD)));
            if (csv.empty()) {
                appendOutputW(L"(empty CSV line)\r\n");
                return 0;
            }
            runLine("ADD COURSE " + csv);
            return 0;
        }
        if (id == IDC_BTN_UPDATE) {
            std::string code = wideToUtf8(getEditText(GetDlgItem(hwnd, IDC_ED_UPCODE)));
            std::string sec = wideToUtf8(getEditText(GetDlgItem(hwnd, IDC_ED_UPSEC)));
            std::string field = wideToUtf8(getEditText(GetDlgItem(hwnd, IDC_ED_FIELD)));
            std::string val = wideToUtf8(getEditText(GetDlgItem(hwnd, IDC_ED_UPVAL)));
            if (code.empty() || field.empty() || val.empty()) {
                appendOutputW(L"(need code, FIELD, value)\r\n");
                return 0;
            }
            std::string line = "UPDATE " + code;
            if (!sec.empty()) {
                line += " SECTION " + sec;
            }
            line += " " + field + " " + val;
            runLine(line);
            return 0;
        }
        if (id == IDC_BTN_DELETE) {
            std::string code = wideToUtf8(getEditText(GetDlgItem(hwnd, IDC_ED_DELCODE)));
            std::string sec = wideToUtf8(getEditText(GetDlgItem(hwnd, IDC_ED_DELSEC)));
            if (code.empty()) {
                appendOutputW(L"(need course code)\r\n");
                return 0;
            }
            std::string line = "DELETE " + code;
            if (!sec.empty()) {
                line += " SECTION " + sec;
            }
            runLine(line);
            return 0;
        }
        if (id == IDC_BTN_SENDRAW) {
            std::string raw = wideToUtf8(getEditText(GetDlgItem(hwnd, IDC_ED_RAW)));
            if (raw.empty()) {
                appendOutputW(L"(empty raw line)\r\n");
                return 0;
            }
            runLine(raw);
            return 0;
        }
        return 0;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = 640;
        mmi->ptMinTrackSize.y = 520;
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

} // namespace

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, PWSTR, int show) {
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);

    initDataDirectoryNearExe();

    const wchar_t kClass[] = L"TimetableGuiClient";

    WNDCLASSW wc{};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.lpszClassName = kClass;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(0, kClass, L"Timetable inquiry - local test client (DCN Assignment 2)", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                                CW_USEDEFAULT, 820, 720, nullptr, nullptr, hi, nullptr);
    if (!hwnd) {
        return 1;
    }

    ShowWindow(hwnd, show);
    UpdateWindow(hwnd);

    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
