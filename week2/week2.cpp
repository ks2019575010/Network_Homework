#include "lib.h"  
#include <fstream>  // 파일 입출력을 위한 헤더 파일을 포함합니다.
#include <sstream>  // 문자열 스트림을 위한 헤더 파일을 포함합니다.

std::string readHTMLFile(const std::string& filename);  // HTML 파일을 읽어오는 함수를 선언합니다.

int main() {  

    WSAData wsaData;  // 윈속 초기화를 위한 데이터 구조체를 선언합니다.
    WSAStartup(MAKEWORD(2, 2), &wsaData);  // 윈속 초기화를 수행합니다.

    SOCKET servsock = socket(AF_INET, SOCK_STREAM, 0);  // 소켓을 생성합니다.
    if (servsock == INVALID_SOCKET) {  // 소켓 생성이 실패한 경우
        cout << "socket() error" << endl;  // 오류 메시지를 출력합니다.
        return 0;  // 프로그램을 종료합니다.
    }

    u_long on = 1;  // 논블로킹 소켓 옵션을 설정하기 위한 변수를 선언합니다.
    if (ioctlsocket(servsock, FIONBIO, &on) == SOCKET_ERROR) {  // 소켓을 논블로킹 모드로 설정합니다.
        cout << "ioctlsocket() error" << endl;  // 오류 메시지를 출력합니다.
        return 0;  // 프로그램을 종료합니다.
    }

    SOCKADDR_IN servaddr;  // 서버 주소 구조체를 선언합니다.
    memset(&servaddr, 0, sizeof(servaddr));  // 구조체를 초기화합니다.
    servaddr.sin_family = AF_INET;  // 주소 패밀리를 설정합니다.
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);  // IP 주소를 설정합니다.
    servaddr.sin_port = htons(12345);  // 포트를 설정합니다.

    if (bind(servsock, (SOCKADDR*)&servaddr, sizeof(servaddr)) == SOCKET_ERROR) {  // 소켓에 주소를 할당합니다.
        cout << "bind() error" << endl;  // 오류 메시지를 출력합니다. 같은 포트를 사용하는 경우에 발생할 수 있습니다.
        return 0;  // 프로그램을 종료합니다.
    }

    if (listen(servsock, SOMAXCONN) == SOCKET_ERROR) {  // 소켓을 수신 대기 상태로 설정합니다.
        cout << "listen() error" << endl;  // 오류 메시지를 출력합니다.
        return 0;  // 프로그램을 종료합니다.
    }

    std::map<std::string, std::string> htmlFiles = {  // HTML 파일을 매핑하는 맵을 생성합니다.
        {"/page1.html", "page1.html"},
        {"/page2.html", "page2.html"},
        {"/page3.html", "page3.html"},
        {"/page4.html", "page4.html"},
        {"/page5.html", "page5.html"}
    };

    while (true) {  // 무한 루프를 시작합니다.
        SOCKADDR_IN cliaddr;  // 클라이언트 주소 구조체를 선언합니다.
        int addrlen = sizeof(cliaddr);  // 클라이언트 주소 구조체의 크기를 설정합니다.
        SOCKET clisock = accept(servsock, (SOCKADDR*)&cliaddr, &addrlen);  // 클라이언트의 연결을 수락합니다.
        if (clisock == INVALID_SOCKET) {  // 클라이언트 연결에 문제가 있는 경우
            if (WSAGetLastError() == WSAEWOULDBLOCK) {  // 논블로킹 소켓이므로 연결 대기 중이면 다시 시도합니다.
                continue;  // 다음 루프로 넘어갑니다.
            }
            else {  // 그 외의 오류인 경우
                cout << "accept() error" << endl;  // 오류 메시지를 출력합니다.
                return 0;  // 프로그램을 종료합니다.
            }
        }

        cout << "Client Connected" << endl;  // 클라이언트가 연결되었다는 메시지를 출력합니다.

        std::string request;  // 요청을 저장할 문자열 변수를 선언합니다.
        char buf[1024] = "";  // 버퍼를 초기화합니다.
        int recvlen;  // 수신한 데이터의 길이를 저장하는 변수를 선언합니다.

        recvlen = recv(clisock, buf, sizeof(buf), 0);  // 클라이언트로부터 요청을 읽어옵니다.
        if (recvlen == SOCKET_ERROR) {  // 데이터 수신에 실패한 경우
            cout << "recv() error" << endl;  // 오류 메시지를 출력합니다.
            closesocket(clisock);  // 소켓을 닫습니다.
            cout << "Client Disconnected" << endl;  // 클라이언트가 연결을 끊었다는 메시지를 출력합니다.
            continue;  // 다음 루프로 넘어갑니다.
        }
        else if (recvlen == 0) {  // 클라이언트가 접속을 끊은 경우
            closesocket(clisock);  // 소켓을 닫습니다.
            cout << "Client Disconnected" << endl;  // 클라이언트가 연결을 끊었다는 메시지를 출력합니다.
            continue;  // 다음 루프로 넘어갑니다.
        }

        buf[recvlen] = '\0';  // 수신한 데이터를 문자열로 만듭니다.
        request = std::string(buf);  // 수신한 데이터를 요청 변수에 저장합니다.

        // 요청 파싱
        std::istringstream iss(request);  // 요청을 문자열 스트림으로 변환합니다.
        std::string method, url, http_version;  // 요청에서 추출할 변수들을 선언합니다.
        iss >> method >> url >> http_version;  // 요청에서 필요한 정보들을 추출합니다.

        std::string response;  // 응답을 저장할 문자열 변수를 선언합니다.
        // 요청된 URL에 해당하는 HTML 파일 전송
        if (htmlFiles.find(url) != htmlFiles.end()) {  // 요청된 URL이 맵에 존재하는 경우
            std::string filename = htmlFiles[url];  // 해당 HTML 파일의 이름을 가져옵니다.
            response = readHTMLFile(filename);  // HTML 파일을 읽어옵니다.
            if (response.empty()) {  // 파일을 읽어올 수 없는 경우
                response = "HTTP/1.1 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nFailed to read HTML file";  // 오류 응답을 생성합니다.
            }
            else {  // 파일을 읽어온 경우
                std::string httpResponse = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: " + std::to_string(response.size()) + "\r\n\r\n" + response;  // HTTP 응답 헤더를 생성합니다.
                send(clisock, httpResponse.c_str(), httpResponse.size(), 0);  // 클라이언트에게 응답을 전송합니다.
            }
        }
        else {  // 요청된 URL이 맵에 존재하지 않는 경우
            response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<html><body><h1>404 Not Found</h1></body></html>";  // 404 응답을 생성합니다.
            send(clisock, response.c_str(), response.size(), 0);  // 클라이언트에게 응답을 전송합니다.
        }

        closesocket(clisock);  // 클라이언트 소켓을 닫습니다.
        cout << "Client Disconnected" << endl;  // 클라이언트가 연결을 끊었다는 메시지를 출력합니다.
    }

    closesocket(servsock);  // 서버 소켓을 닫습니다.
    WSACleanup();  // 윈속을 정리합니다.
    return 0;  // 프로그램을 종료합니다.
}

std::string readHTMLFile(const std::string& filename) {  // HTML 파일을 읽어오는 함수를 정의합니다.
    std::ifstream file(filename);  // 파일을 엽니다.
    if (!file.is_open()) {  // 파일을 열지 못한 경우
        std::cerr << "Failed to open file: " << filename << std::endl;  // 오류 메시지를 출력합니다.
        return "";  // 빈 문자열을 반환합니다.
    }

    std::stringstream buffer;  // 파일 내용을 읽을 버퍼를 생성합니다.
    buffer << file.rdbuf();  // 파일 내용을 버퍼에 읽어옵니다.
    return buffer.str();  // 버퍼의 내용을 문자열로 반환합니다.
}