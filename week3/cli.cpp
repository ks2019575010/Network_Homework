#include "lib.h"

#define DEFAULT_BUFLEN 1024

int main() {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    char ip[16] = "";
    cout << "Input server IP: "; 
    cin >> ip;
     

    // 서버 주소 설정
    sockaddr_in serverAddr;
    ZeroMemory(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(ip); // 서버의 IP 주소
    serverAddr.sin_port = htons(12345); // 서버의 포트 번호

    cin.getline(ip, sizeof(ip));//이후 개행문자제거

    // 소켓 생성
    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create socket" << std::endl;
        WSACleanup();
        return 1;
    }

    // 서버에 연결
    if (connect(clientSocket, (SOCKADDR*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Failed to connect to server" << std::endl;
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    char recvBuf[DEFAULT_BUFLEN];
    int recvBufLen = DEFAULT_BUFLEN;

    while (true) {
        // 사용자로부터 메시지 입력 받기
        std::cout << "Enter message: ";
        std::string message;
        std::getline(std::cin, message);

        // 서버로 메시지 보내기
        int bytesSent = send(clientSocket, message.c_str(), message.size(), 0);
        if (bytesSent == SOCKET_ERROR) {
            std::cerr << "send failed with error: " << WSAGetLastError() << std::endl;
            break;
        }
        // 서버로부터 메시지 받기
        int bytesReceived = recv(clientSocket, recvBuf, recvBufLen, 0);
        if (bytesReceived > 0) {
            recvBuf[bytesReceived] = '\0';
            std::cout << "other: " << recvBuf << std::endl;
        } else if (bytesReceived == 0) {
            std::cerr << "Connection closed by server" << std::endl;
            break;
        } else {
            std::cerr << "recv failed with error: " << WSAGetLastError() << std::endl;
            break;
        }
    }

    // 소켓 닫기 및 Winsock 종료
    closesocket(clientSocket);
    WSACleanup();

    return 0;
}
//192.168.219.104
//192.168.1.36