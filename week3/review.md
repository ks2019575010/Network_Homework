launch program does not exist 오류

-week2의 .vscode를 긁어오니 해결되었다. 무엇이 문제였는지 다시 알아볼것.

```cpp
#include "lib.h"

#define DEFAULT_BUFLEN 1024
#define MAX_THREAD 8

// 세션 정보를 담는 구조체
struct Session {
    SOCKET sock = INVALID_SOCKET; // 클라이언트 소켓
    char buf[DEFAULT_BUFLEN] = {}; // 데이터를 읽고 쓰는 버퍼
    WSAOVERLAPPED readOverLapped = {}; // 비동기 읽기를 위한 오버랩 구조체
    WSAOVERLAPPED writeOverLapped = {}; // 비동기 쓰기를 위한 오버랩 구조체

    // 생성자
    Session() {}
    Session(SOCKET sock) : sock(sock) {}
};

//vector<Session*> sessions;//여기서 전역변수 선언해야하나?

// 스레드 풀 종료 여부를 나타내는 원자적 변수
atomic<bool> TPoolRunning = true;

// 메모리 풀 객체 선언 및 초기화
MemoryPool* MemPool = new MemoryPool(sizeof(Session), 1000);

// WorkerThread 함수의 프로토타입 선언
void WorkerThread(HANDLE iocpHd);

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
    vector<Session*> sessions;//여기서 선언 sessions
    sessions.reserve(100);

    // WorkerThread를 생성하여 스레드 풀 생성
    for (int i = 0; i < MAX_THREAD; i++) {
        HANDLE hThread = CreateThread(
            NULL, 0,
            (LPTHREAD_START_ROUTINE)WorkerThread,
            iocp, 0, NULL
        );
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

        // 비블록킹 모드로 설정
        u_long on = 1;
        if (ioctlsocket(clisock, FIONBIO, &on) == SOCKET_ERROR) {
            cout << "ioctlsocket() error" << endl;
            return 1;
        }

        // 세션 객체를 메모리 풀에서 할당하여 초기화
        Session* session = MemPool_new<Session>(*MemPool, clisock);
        sessions.push_back(session);

        cout << "Client Connected" << endl;

        // IOCP와 소켓을 연결
        CreateIoCompletionPort((HANDLE)session->sock, iocp, (ULONG_PTR)session, 0);

        // 비동기 수신 시작
        WSABUF wsabuf_R = {};
        wsabuf_R.buf = session->buf;
        wsabuf_R.len = DEFAULT_BUFLEN;
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
    vector<Session*> sessions;//추가해야하나?
    DWORD flags = 0;

    while (TPoolRunning) {
        // 완료된 IOCP 작업을 가져옴
        bool ret = GetQueuedCompletionStatus(
            iocpHd, &bytesTransfered,
            (ULONG_PTR*)&session, &lpOverlapped, INFINITE
        );
        if (!ret || bytesTransfered == 0) {
            cout << "Client Disconnected" << endl;
            closesocket(session->sock);
            MemPool_delete(*MemPool, session);
            continue;
        }

        // 완료된 작업에 따라 처리
        if (lpOverlapped == &session->readOverLapped) {
            // 읽기 완료 후 데이터를 쓰는 작업 시작
            wsabuf_S.buf = session->buf;
            wsabuf_S.len = bytesTransfered;

            // 클라이언트가 보낸 메시지를 다른 클라이언트에게 브로드캐스트
            for (auto otherSession : sessions) {
                if (otherSession != session) {
                    WSASend(
                        otherSession->sock, &wsabuf_S, 1,
                        NULL, 0, &otherSession->writeOverLapped, NULL
                    );
                }
            }

            // 다시 클라이언트로부터 메시지를 받을 수 있도록 수신 작업 시작
            WSARecv(
                session->sock, &wsabuf_R, 1,
                NULL, &flags, &session->readOverLapped, NULL
            );
        }
        else if (lpOverlapped == &session->writeOverLapped) {
            // 쓰기 완료 후 다시 읽는 작업 시작
            wsabuf_R.buf = session->buf;
            wsabuf_R.len = DEFAULT_BUFLEN;
            DWORD flags = 0;
            WSARecv(
                session->sock, &wsabuf_R, 1,
                NULL, &flags, &session->readOverLapped, NULL
            );
        }
    }
}
```

이후 클라이언트 2개를 실행해 서버에 연결한뒤 send한 결과, 무한로딩이 발생했다.

chat gpt에 의하면

문제가 발생한 이유는 클라이언트에서 데이터를 전송할 때, 서버가 이를 처리하는 부분이 구현되어 있지 않기 때문입니다. 

클라이언트는 데이터를 서버로 전송할 때 send 함수를 사용하고, 서버는 이 데이터를 받아 처리해야 합니다.

따라서 클라이언트가 메시지를 보내면, 서버는 이를 수신하여 다른 클라이언트에게 브로드캐스트해야 합니다.

현재 서버 코드에서는 클라이언트가 데이터를 보내면 그 데이터를 다른 클라이언트에게 브로드캐스트하는 부분이 구현되어 있지 않습니다. 이를 추가해야 합니다.


