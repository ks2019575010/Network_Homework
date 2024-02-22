#include "lib.h"

#define DEFAULT_BUFLEN 1024
#define MAX_THREAD 8

// 세션 정보를 담는 구조체
struct Session {
    SOCKET sock = INVALID_SOCKET; // 클라이언트 소켓
    char buf[DEFAULT_BUFLEN] = {}; // 데이터를 읽고 쓰는 버퍼
    WSAOVERLAPPED readOverLapped = {}; // 비동기 읽기를 위한 오버랩 구조체
    WSAOVERLAPPED writeOverLapped = {}; // 비동기 쓰기를 위한 오버랩 구조체

    Session(SOCKET sock) : sock(sock){}//session의 생성자 명시
};

// 세션 객체를 담을 벡터 생성 (전역 벡터로 선언)
vector<Session*> sessions;

// 쓰레드 풀 종료 여부를 나타내는 원자적 변수
atomic<bool> TPoolRunning = true;

// WorkerThread 함수의 프로토타입 선언
void WorkerThread(HANDLE iocpHd);

// 메모리 풀 객체 선언 및 초기화
MemoryPool* MemPool = new MemoryPool(sizeof(Session), 1000);

int main() {
    // Winsock 초기화
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);

    // 서버 소켓 생성
    SOCKET servsock = WSASocket(
        AF_INET, SOCK_STREAM, 0,
        NULL, 0, WSA_FLAG_OVERLAPPED
    );
    if (servsock == INVALID_SOCKET) {
        cout << "WSASocket() error" << endl;
        return 1;
    }

    // 비블록킹 모드로 설정
    u_long on = 1;
    if (ioctlsocket(servsock, FIONBIO, &on) == SOCKET_ERROR) {
        cout << "ioctlsocket() error" << endl;
        return 1;
    }

    // 서버 주소 설정 및 바인딩
    SOCKADDR_IN servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(12345);

    if (bind(servsock, (SOCKADDR*)&servaddr, sizeof(servaddr)) == SOCKET_ERROR) {
        cout << "bind() error" << endl;
        return 1;
    }

    // 연결 대기열 생성
    if (listen(servsock, SOMAXCONN) == SOCKET_ERROR) {
        cout << "listen() error" << endl;
        return 1;
    }

    // IOCP 객체 생성
    HANDLE iocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

    // 세션 객체를 담을 벡터 생성
    //vector<Session*> sessions;//왜 전역변수로 하면 안될까
    sessions.reserve(100);//전역변수에 동적할당

    // WorkerThread를 생성하여 스레드 풀 생성
    for (int i = 0; i < MAX_THREAD; i++) {
        HANDLE hThread = CreateThread(
            NULL, 0,
            (LPTHREAD_START_ROUTINE)WorkerThread,
            iocp, 0, NULL
        );
        //쓰레드핸들을 닫음
        CloseHandle(hThread);
    }

    // 클라이언트 연결을 받고 처리하는 부분
    while (true) {
        SOCKADDR_IN clientaddr;
        int addrlen = sizeof(clientaddr);

        // 클라이언트의 연결을 받음
        SOCKET clisock = accept(servsock, NULL, NULL);
        if (clisock == INVALID_SOCKET) {
            if (WSAGetLastError() != WSAEWOULDBLOCK) {
                cout << "accept() error" << endl;
                return 1;
            }
            continue;
        }
        //?

        // 세션 객체를 메모리 풀에서 할당하여 초기화
        Session* session = MemPool_new<Session>(*MemPool, clisock);

        // 세션을 세션 벡터에 추가
        sessions.push_back(session);

        cout << "Client Connected" << endl;

        // IOCP와 소켓을 연결
        CreateIoCompletionPort((HANDLE)session->sock, iocp, (ULONG_PTR)session, 0);

        // 비동기 수신 시작
        WSABUF wsabuf_R = {};
        wsabuf_R.buf = session->buf; // 데이터를 저장할 버퍼 설정
        wsabuf_R.len = DEFAULT_BUFLEN; // 버퍼의 크기 설정
        DWORD flags = 0;
        WSARecv(
            clisock, &wsabuf_R, 1,
            NULL, &flags, &session->readOverLapped, NULL
        );
    }

    // 세션 벡터의 모든 세션을 정리하고 메모리 풀에서 해제
    for (auto session : sessions) {
        closesocket(session->sock);
        MemPool_delete(*MemPool, session);
    }

    // 스레드 풀 종료
    TPoolRunning = false;
    CloseHandle(iocp);

    // 서버 소켓 닫기 및 Winsock 종료
    closesocket(servsock);
    WSACleanup();
    return 0;
}

// IOCP에서 동작할 WorkerThread 함수
void WorkerThread(HANDLE iocpHd) {
    DWORD bytesTransfered;
    Session* session;
    LPOVERLAPPED lpOverlapped;
    WSABUF wsabuf_S = {}, wsabuf_R = {};
    //vector<Session*> sessions;//추가해야하나?
    DWORD flags = 0;

    while (TPoolRunning) {
        // 완료된 IOCP 작업을 가져옴
        //cout << "iocp 가져옴" << endl;//확인용

        bool ret = GetQueuedCompletionStatus(
            iocpHd, &bytesTransfered,
            (ULONG_PTR*)&session, &lpOverlapped, INFINITE
        );
        if (!ret || bytesTransfered == 0)  {
            cout << "Client Disconnected" << endl;//정상출력
            closesocket(session->sock);
            MemPool_delete(*MemPool, session);
            //sessions.erase(std::remove(sessions.begin(), sessions.end(), session), sessions.end());
            continue;
        }
        //cout << "163" << endl;
        // 완료된 작업에 따라 처리
        if (lpOverlapped == &session->readOverLapped) {//이 조건문은 현재 완료된 IO 작업이 해당 세션의 읽기 작업인지를 판별하여 그에 따른 처리를 수행
            // 읽기 완료 후 데이터를 쓰는 작업 시작
            
            //cout.write(session->buf, bytesTransfered);//출력되지 않음, 읽기가 완료되질 않았거나 buf에 값이 안들어옴
            //cout << endl;
            //cout << "170" << endl;//읽기하는지 확인용, 출력안됨, 읽기작업조차 일어나지 않음

            wsabuf_S.buf = session->buf;//wsabuf_S = 클라이언트가 보낸 데이터가 저장된 버퍼
            wsabuf_S.len = bytesTransfered;//

            // 클라이언트가 보낸 메시지를 다른 클라이언트에게 브로드캐스트
            for (auto otherSession : sessions) {//전체 클라이언트 수의 자신을 제외한 모든 클라이언트?
                if (otherSession != session) {//othersession이 자신이 아니라면
                    WSASend(
                        otherSession->sock, &wsabuf_S, 1,
                        NULL, 0, &otherSession->writeOverLapped, NULL
                    );
                }
            }

            // 다시 클라이언트로부터 메시지를 받을 수 있도록 수신 작업 시작
            //Exception has occurred. Segmentation fault오류
            //원인
            //1. null 값을 가리키는 포인터에 접근할 경우
            //2. 할당 받은 메모리 공간을 넘은 곳을 건드린 경우
            //3. 더 이상 존재하지 않는 메모리 영역을 가리킬 경우
            //4. read-only 표시 메모리 영역에 쓰려고 할 경우
            WSARecv(//3번이 아니라면 밑에있는것도 아니어야...flag문제인가?
                session->sock, &wsabuf_R, 1,
                NULL, &flags, &session->readOverLapped, NULL
            );
        }
        else if (lpOverlapped == &session->writeOverLapped) {
            //cout << "192" << endl;
            // 쓰기 완료 후 다시 읽는 작업 시작
            wsabuf_R.buf = session->buf;
            wsabuf_R.len = DEFAULT_BUFLEN;
            //DWORD flags = 0;
            WSARecv(
                session->sock, &wsabuf_R, 1,
                NULL, &flags, &session->readOverLapped, NULL
            );
        }
    }
}