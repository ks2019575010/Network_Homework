#include "lib.h" 
#include <fstream>  
#include <sstream>  // 문자열 스트림을 위한 헤더 파일

std::string readHTMLFile(const std::string& filename);  // HTML 파일을 읽어오는 함수 선언

int main() {  // 메인 함수 시작

    WSAData wsaData;  // 윈속 초기화를 위한 데이터 구조체 선언
    WSAStartup(MAKEWORD(2, 2), &wsaData);  // 윈속 초기화

    SOCKET servsock = socket(AF_INET, SOCK_STREAM, 0);  // 소켓 생성
    if (servsock == INVALID_SOCKET) {  // 소켓 생성 실패 시
        cout << "socket() error" << endl;  // 오류 메시지 출력
        return 0;  // 프로그램 종료
    }

    u_long on = 1;  // 논블로킹 소켓 옵션을 설정하기 위한 변수
    if (ioctlsocket(servsock, FIONBIO, &on) == SOCKET_ERROR) {  // 논블로킹 소켓으로 설정
        cout << "ioctlsocket() error" << endl;  // 오류 메시지 출력
        return 0;  // 프로그램 종료
    }

    SOCKADDR_IN servaddr;  // 서버 주소 구조체 선언
    memset(&servaddr, 0, sizeof(servaddr));  // 구조체 초기화
    servaddr.sin_family = AF_INET;  // 주소 패밀리 설정
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);  // IP 주소 설정
    servaddr.sin_port = htons(12345);  // 포트 설정

    if (bind(servsock, (SOCKADDR*)&servaddr, sizeof(servaddr)) == SOCKET_ERROR) {  // 소켓에 주소 할당
        cout << "bind() error" << endl;  // 오류 메시지 출력, 같은 포트일 경우 나타남
        return 0;  // 프로그램 종료
    }

    if (listen(servsock, SOMAXCONN) == SOCKET_ERROR) {  // 소켓을 수신 대기 상태로 설정
        cout << "listen() error" << endl;  // 오류 메시지 출력
        return 0;  // 프로그램 종료
    }

    std::map<std::string, std::string> htmlFiles = {  // HTML 파일 매핑
        {"/page1.html", "page1.html"},
        {"/page2.html", "page2.html"},
        {"/page3.html", "page3.html"},
        {"/page4.html", "page4.html"},
        {"/page5.html", "page5.html"}
    };

    while (true) {
        SOCKADDR_IN cliaddr; // 클라이언트 주소 구조체 선언
        int addrlen = sizeof(cliaddr); // 클라이언트 주소 구조체의 크기 설정
        SOCKET clisock = accept(servsock, (SOCKADDR*)&cliaddr, &addrlen); // 클라이언트의 연결을 받아들임
        if (clisock == INVALID_SOCKET) { // 클라이언트 연결에 문제가 있는 경우
            if (WSAGetLastError() == WSAEWOULDBLOCK) { // 논블로킹 소켓이므로 연결 대기중이면 다시 시도
                continue;
            }
            else { // 그 외의 오류인 경우
                cout << "accept() error" << endl; // 오류 메시지 출력
                return 0; // 프로그램 종료
            }
        }

        cout << "Client Connected" << endl; // 클라이언트가 연결되었다는 메시지 출력

        std::string request;
        char buf[1024] = ""; // 버퍼 초기화

       // 클라이언트로부터 요청 읽기
        while (true) {
            int bytesReceived = recv(clisock, buf, sizeof(buf), 0);
            if (bytesReceived <= 0) {
                break;
            }
            request += std::string(buf, buf + bytesReceived);
            // 빈 줄을 만나면 요청 읽기 종료
            if (request.find("\r\n\r\n") != std::string::npos) {
                break;
            }
        }

        // 요청 파싱
        std::istringstream iss(request);
        std::string method, url, http_version;
        iss >> method >> url >> http_version;

        // 요청된 URL에 해당하는 HTML 파일 전송
        if (htmlFiles.find(url) != htmlFiles.end()) {
            std::string filename = htmlFiles[url];
            std::string response = readHTMLFile(filename);
            send(clisock, response.c_str(), response.size(), 0);
        } else {
            std::string response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<html><body><h1>404 Not Found</h1></body></html>";
            send(clisock, response.c_str(), response.size(), 0);
        }
    }

    closesocket(servsock);  // 서버 소켓 닫기
    WSACleanup();  // 윈속 정리
    return 0;  // 프로그램 종료
}

std::string readHTMLFile(const std::string& filename) {  // HTML 파일 읽기 함수 정의
    std::ifstream file(filename);  // 파일 열기
    if (!file.is_open()) {  // 파일 열기 실패 시
        std::cerr << "Failed to open file: " << filename << std::endl;  // 오류 메시지 출력
        return "Failed to open file: " + filename;  // 오류 반환
    }

    std::string content((std::istreambuf_iterator<char>(file)), (std::istreambuf_iterator<char>()));  // 파일 내용 읽기
    return content;  // 파일 내용 반환
}