#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "database.h" // 成员2提供的头文件

#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int PORT = 8888;

// 处理单个客户端连接的线程函数
void handleClient(SOCKET clientSocket) {
    ClientSession session; // 成员2要求：每个TCP连接维护一个Session
    char buffer[4096];
    string accumulatedData = "";

    cout << "[Server] New client connected. Thread ID: " << this_thread::get_id() << endl;

    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived <= 0) {
            cout << "[Server] Client disconnected." << endl;
            break;
        }
        
        buffer[bytesReceived] = '\0';
        accumulatedData += buffer;

        // 处理可能由于 TCP 粘包带来的多条或单条指令（按 \n 分割）
        size_t pos;
        while ((pos = accumulatedData.find('\n')) != string::npos) {
            string request = accumulatedData.substr(0, pos);
            accumulatedData.erase(0, pos + 1);

            // 移除可能存在的 \r (Windows 换行符带来的遗留)
            if (!request.empty() && request.back() == '\r') {
                request.pop_back();
            }

            if (request.empty()) continue;

            cout << "[Server] Received request: " << request << endl;

            // 调用成员2的数据库逻辑
            string response = handleRequest(request, session);

            // 协议设计：在响应末尾添加统一的结束符 __EOF__
            string finalResponse = response + "\n__EOF__\n";

            // 发送回客户端
            send(clientSocket, finalResponse.c_str(), finalResponse.length(), 0);
        }
    }
    closesocket(clientSocket);
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed." << endl;
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed." << endl;
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(PORT);

    if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Bind failed." << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
        cerr << "Listen failed." << endl;
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    cout << "=== Timetable Server Started on Port " << PORT << " ===" << endl;
    cout << "Waiting for clients to connect..." << endl;

    // 主循环：接受客户端连接并创建新线程
    while (true) {
        SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);
        if (clientSocket == INVALID_SOCKET) {
            cerr << "Accept failed." << endl;
            continue;
        }
        // 使用 std::thread 处理并发 (满足 Minimum 5 concurrent connections 需求)
        thread clientThread(handleClient, clientSocket);
        clientThread.detach(); // 分离线程，任其在后台独立运行
    }

    closesocket(serverSocket);
    WSACleanup();
    return 0;
}