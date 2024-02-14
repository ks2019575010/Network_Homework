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
        int recvlen; // 수신한 데이터의 길이를 저장하는 변수입니다.

       // 클라이언트로부터 요청 읽기
        while (true) { // 데이터를 수신하는 루프입니다.
                recvlen = recv(clisock, buf, sizeof(buf), 0); // 클라이언트로부터 데이터를 수신합니다.
                if (recvlen == SOCKET_ERROR) { // 데이터 수신에 실패한 경우
                    // 논블로킹 소켓은 recv()에서 한번 더 체크해줘야함
                    if (WSAGetLastError() == WSAEWOULDBLOCK) { // 더 이상 수신할 데이터가 없는 경우
                        continue; // 다음 루프로 넘어갑니다.
                    }
                    else {
                        cout << "recv() error" << endl; // 오류 메시지를 출력합니다.
                        return 0; // 프로그램을 종료합니다.
                    }
                }
                else { // 데이터를 성공적으로 수신한 경우
                    break; // 루프를 탈출합니다.
                }
            }

            if (recvlen == 0) { // 클라이언트가 접속을 끊은 경우
                closesocket(clisock); // 클라이언트 소켓을 닫습니다.
                cout << "Client Disconnected" << endl; // 클라이언트가 연결을 끊었다는 메시지를 출력합니다.
                break; // 루프를 탈출하여 다음 클라이언트의 접속을 대기합니다.
            }

            buf[recvlen] = '\0'; // 수신한 데이터를 문자열로 만듭니다.
            request = std::string(buf);
            //cout << "Recv: " << buf << endl; // 수신한 데이터를 출력합니다.

        // 요청 파싱
        std::istringstream iss(request);// request를 공백을 기준으로 분리하여 각각의 공간에 넣는다.
        std::string method, url, http_version;
        iss >> method >> url >> http_version;

        // 요청된 URL에 해당하는 HTML 파일 전송
        if (htmlFiles.find(url) != htmlFiles.end()) {//매핑한 곳에서 찾았다면, !=가 왜 찾은것을 의미하나 => find가 아무것도 못찾았을때 end를 반환하기 때문에
            cout << url << endl;//url확인용 정상
            std::string filename = htmlFiles[url];
            cout << htmlFiles[url] << endl;//확인용 정상
            std::string response = readHTMLFile(filename);
            cout << response << endl;//확인용 정상, 근데 왜 화면엔 안나오지
            send(clisock, response.c_str(), response.size(), 0);
        } else {//404 not found
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