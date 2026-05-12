#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

const int SERVER_PORT = 8888;
const char* SERVER_IP = "127.0.0.1";

SOCKET clientSocket;

// --- 网络通信核心 ---
// 发送请求，自动附加 \n 满足服务器行读取逻辑
void SendRequest(const string& request) {
    string payload = request + "\n";
    send(clientSocket, payload.c_str(), payload.length(), 0);
}

// 接收响应，直到读到 \n__EOF__\n 标志
string ReceiveResponse() {
    char buffer[4096];
    string accumulatedData = "";
    
    while (true) {
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            accumulatedData += buffer;
            
            // 检查是否收到了结束标志
            size_t eofPos = accumulatedData.find("\n__EOF__\n");
            if (eofPos != string::npos) {
                // 剔除结束标志
                accumulatedData.erase(eofPos);
                break;
            }
        } else {
            return "Error: Disconnected from server.";
        }
    }
    return accumulatedData;
}

// 通用交互函数：发送请求并打印结果
void ExecuteAndPrint(const string& request) {
    SendRequest(request);
    string response = ReceiveResponse();
    cout << "\n---------------- SERVER RESPONSE ----------------\n";
    cout << response << "\n";
    cout << "-------------------------------------------------\n";
}

// --- 菜单模块 ---
void StudentMenu() {
    int choice;
    string input, input2;
    while (true) {
        cout << "\n=== Student Menu ===" << endl;
        cout << "1. Search by Course Code" << endl;
        cout << "2. Search by Instructor" << endl;
        cout << "3. View All Courses" << endl;
        cout << "0. Return to Main Menu" << endl;
        cout << "Enter choice: ";
        cin >> choice;

        if (choice == 0) break;

        // 忽略缓冲区残留的换行符，防止 getline 出错
        cin.ignore(10000, '\n'); 

        switch (choice) {
            case 1:
                cout << "Enter Course Code (e.g., COMP3003): ";
                getline(cin, input);
                ExecuteAndPrint("QUERY CODE " + input);
                break;
            case 2:
                cout << "Enter Instructor Name: ";
                getline(cin, input);
                ExecuteAndPrint("QUERY INSTRUCTOR " + input);
                break;
            case 3:
                cout << "Enter Semester (e.g., 2026S1) or leave blank for all: ";
                getline(cin, input);
                if (input.empty()) ExecuteAndPrint("QUERY ALL");
                else ExecuteAndPrint("QUERY ALL " + input);
                break;
            default:
                cout << "Invalid choice!" << endl;
        }
    }
}

bool AdminLogin() {
    string username, password;
    cout << "Enter Admin Username: ";
    cin >> username;
    cout << "Enter Password: ";
    cin >> password;

    string request = "LOGIN " + username + " " + password;
    SendRequest(request);
    
    string response = ReceiveResponse();
    if (response.find("SUCCESS") != string::npos) {
        cout << "Login Successful!" << endl;
        return true;
    } else {
        cout << "Login Failed: " << response << endl;
        return false;
    }
}

void AdminMenu() {
    if (!AdminLogin()) return;

    int choice;
    string code, section, field, value, csv;
    while (true) {
        cout << "\n=== Administrator Menu ===" << endl;
        cout << "1. Add Course" << endl;
        cout << "2. Update Course" << endl;
        cout << "3. Delete Course" << endl;
        cout << "0. Logout" << endl;
        cout << "Enter choice: ";
        cin >> choice;
        cin.ignore(10000, '\n');

        if (choice == 0) {
            ExecuteAndPrint("LOGOUT");
            break;
        }

        switch (choice) {
            case 1:
                cout << "Enter course details CSV format\n(code,title,section,instructor,day,time,classroom,semester):\n> ";
                getline(cin, csv);
                ExecuteAndPrint("ADD COURSE " + csv);
                break;
            case 2:
                cout << "Enter Course Code: ";
                getline(cin, code);
                cout << "Enter Section (or leave blank if none): ";
                getline(cin, section);
                cout << "Enter Field to update (TITLE/INSTRUCTOR/DAY/TIME/CLASSROOM/SEMESTER): ";
                getline(cin, field);
                cout << "Enter New Value: ";
                getline(cin, value);
                
                if (section.empty()) {
                    ExecuteAndPrint("UPDATE " + code + " " + field + " " + value);
                } else {
                    ExecuteAndPrint("UPDATE " + code + " SECTION " + section + " " + field + " " + value);
                }
                break;
            case 3:
                cout << "Enter Course Code to delete: ";
                getline(cin, code);
                cout << "Enter Section (or leave blank to delete all sections): ";
                getline(cin, section);
                
                if (section.empty()) {
                    ExecuteAndPrint("DELETE " + code);
                } else {
                    ExecuteAndPrint("DELETE " + code + " SECTION " + section);
                }
                break;
            default:
                cout << "Invalid choice!" << endl;
        }
    }
}

// --- 主程序 ---
int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed." << endl;
        return 1;
    }

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    cout << "Connecting to server at " << SERVER_IP << ":" << SERVER_PORT << "..." << endl;
    if (connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        cerr << "Connection to server failed. Is the server running?" << endl;
        WSACleanup();
        return 1;
    }
    cout << "Connected successfully!\n" << endl;

    int mode;
    while (true) {
        cout << "\n=== Course Timetable System ===" << endl;
        cout << "1. Student Mode (Query Only)" << endl;
        cout << "2. Administrator Mode (Full Access)" << endl;
        cout << "0. Exit" << endl;
        cout << "Choose role: ";
        cin >> mode;

        if (mode == 1) {
            StudentMenu();
        } else if (mode == 2) {
            AdminMenu();
        } else if (mode == 0) {
            break;
        } else {
            cout << "Invalid mode! Please try again." << endl;
            cin.clear();
            cin.ignore(10000, '\n');
        }
    }

    closesocket(clientSocket);
    WSACleanup();
    return 0;
}