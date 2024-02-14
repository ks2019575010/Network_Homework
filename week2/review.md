자체리뷰

<pre><code>
#include "lib.h"   
#include <fstream>  
#include <sstream>  

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
        cout << "bind() error" << endl;  // 오류 메시지 출력
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

    while (true) {  // 무한 루프 시작
        SOCKADDR_IN cliaddr;  // 클라이언트 주소 구조체 선언
        int addrlen = sizeof(cliaddr);  // 주소 구조체 크기 설정
        SOCKET clisock = accept(servsock, (SOCKADDR*)&cliaddr, &addrlen);  // 연결 수락
        if (clisock == INVALID_SOCKET) {  // 연결 수락 실패 시
            if (WSAGetLastError() == WSAEWOULDBLOCK) {  // 연결이 비동기로 이루어지는 경우
                continue;  // 루프 계속
            }
            else {  // 그 외의 경우
                cout << "accept() error" << endl;  // 오류 메시지 출력
                return 0;  // 프로그램 종료
            }
        }

        cout << "Client Connected" << endl;  // 클라이언트 연결 성공 메시지 출력

        char request[1024];  // 요청 메시지를 저장하기 위한 버퍼
        recv(clisock, request, sizeof(request), 0);  // 요청 메시지 수신
        std::string requestStr(request);  // 요청 메시지를 문자열로 변환

        // 요청 문자열에서 URL 추출
        std::istringstream iss(requestStr);  // 문자열 스트림 생성
        std::string method, url, protocol;  // 요청 메서드, URL, 프로토콜을 저장할 변수
        iss >> method >> url >> protocol;  // 공백으로 구분된 요청 메시지를 분석하여 변수에 저장

        std::string htmlContent;  // HTML 내용을 저장할 변수

        if (htmlFiles.find(url) != htmlFiles.end()) {  // 요청된 URL이 존재하는 경우
            htmlContent = readHTMLFile(htmlFiles[url]);  // HTML 파일 읽기
        } else {  // 요청된 URL이 존재하지 않는 경우
            htmlContent = "404 Not Found";  // 404 오류 메시지 설정
        }

        int sendlen = send(clisock, htmlContent.c_str(), htmlContent.size(), 0);  // 클라이언트에게 HTML 전송
        if (sendlen == SOCKET_ERROR) {  // 전송 실패 시
            cout << "send() error" << endl;  // 오류 메시지 출력
        }
        else {  // 전송 성공 시
            cout << "HTML Sent to Client" << endl;  // 전송 완료 메시지 출력
        }

        closesocket(clisock);  // 클라이언트 소켓 닫기
        cout << "Client Disconnected" << endl;  // 클라이언트 연결 해제 메시지 출력

        break;  // 루프 종료
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
</code></pre>

Client Connected

HTML Sent to Client

Client Disconnected
가 터미널에 출력됨

하지만 클라이언트에서 html페이지가 실행되지 않고 
"localhost에서 잘못된 응답을 전송했습니다." 라고 나온뒤
서버가 닫히면서 "localhost에서 연결을 거부했다"고 나옴

문제점예상

1.서버가 너무 빨리 닫히는게 문제인가?

2.html을 열 수 있게하는 코드가 문제인가?

3.내가 알지못하는 무언가가 문제인가?

서버를 빨리 닫히지 않기게 하기 위해 클라이언트가 연결중일때는 서버가 닫히지 않게 하
Connected와 Disconnecte가 무한히 반복하기 시작했다.

<pre><code>{code}</code></pre>
