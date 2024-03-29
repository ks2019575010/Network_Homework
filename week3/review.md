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

        char buf[1024] = "";

        while (true) {
            cout << "Input: ";
            cin.getline(buf, sizeof(buf)); // 개행 문자까지 포함하여 한 줄 전체를 읽어옴

            // 논블로킹 소켓은 send()도 루프를 돌면서 될 때까지 계속 시도해야함
            while (true) {
                if (send(clisock, buf, strlen(buf) + 1, 0) == SOCKET_ERROR) {
                    // 논블로킹 소켓은 send()에서 한번 더 체크해줘야함
                    if (WSAGetLastError() == WSAEWOULDBLOCK) {
                        continue;
                    }
                    else {
                        cout << "send() error" << endl;
                        return 0;
                    }
                }
                else {
                    break;
                }
            }

            int recvlen;
            // 논블로킹 소켓은 recv()도 루프를 돌면서 될 때까지 계속 시도해야함
            while (true) {
                recvlen = recv(clisock, buf, sizeof(buf), 0);
                if (recvlen == SOCKET_ERROR) {
                    // 논블로킹 소켓은 recv()에서 한번 더 체크해줘야함
                    if (WSAGetLastError() == WSAEWOULDBLOCK) {
                        continue;
                    }
                    else {
                        cout << "recv() error" << endl;
                        return 0;
                    }
                }
                else {
                    break;
                }
            }

            if (recvlen == 0) {
                break;
            }

            buf[recvlen] = '\0';

            cout << "Echo: " << buf << endl;
        }
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

해당 서버와 클라이언트 간에 채팅은 되지만 뛰어쓰기는 인식을 못하는듯 recv() error가 클라이언트 1에게 전달
서로 다른 클라이언트간의 소통은 되지 않았다.
이후 다른 연결된 클라이언트로 채팅을 진행 하려 했지만 서버에서 input이 진행되지 않고 send error, 클라이언트에 recv error가 나며 종료했다.

일단 클라이언트가 클라이언트에게 메세지를 전달 할 수 있게 고쳐야 한다.

이하 cht gpt 변경점

1.sessions 벡터를 WorkerThread에서 사용할 수 있도록 전역 벡터로 선언해야 합니다. 이렇게 하면 클라이언트 세션이 새로 연결될 때마다 WorkerThread에서 해당 세션을 처리할 수 있습니다.

2.클라이언트가 연결될 때마다 세션 객체를 sessions 벡터에 추가해야 합니다.

3.클라이언트가 데이터를 보낼 때마다 이 데이터를 다른 모든 클라이언트에게 브로드캐스트해야 합니다.

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
    //vector<Session*> sessions;//추가해야하나?
    //DWORD flags = 0;

    while (TPoolRunning) {
        // 완료된 IOCP 작업을 가져옴
        bool ret = GetQueuedCompletionStatus(
            iocpHd, &bytesTransfered,
            (ULONG_PTR*)&session, &lpOverlapped, INFINITE
        );
        if (!ret || bytesTransfered == 0)  {
            cout << "Client Disconnected" << endl;
            closesocket(session->sock);
            MemPool_delete(*MemPool, session);
            //sessions.erase(std::remove(sessions.begin(), sessions.end(), session), sessions.end());
            continue;
        }

        // 완료된 작업에 따라 처리
        if (lpOverlapped == &session->readOverLapped) {//이 조건문은 현재 완료된 IO 작업이 해당 세션의 읽기 작업인지를 판별하여 그에 따른 처리를 수행
            // 읽기 완료 후 데이터를 쓰는 작업 시작
            wsabuf_S.buf = session->buf;//
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
            WSARecv(
                session->sock, &wsabuf_R, 1,
                NULL, 0, &session->readOverLapped, NULL
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

고친다고 고쳤는데 여전히 왜 안돼는지 모르겠다.
socket할당을 하지 않아서

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
    //DWORD flags = 0;

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
            
            cout.write(session->buf, bytesTransfered);//출력되지 않음, 읽기가 완료되질 않았거나 buf에 값이 안들어옴
            cout << endl;
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
            WSARecv(//
                session->sock, &wsabuf_R, 1,
                NULL, 0, &session->readOverLapped, NULL
            );
        }
        else if (lpOverlapped == &session->writeOverLapped) {
            //cout << "192" << endl;
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

이제 //Exception has occurred. Segmentation fault오류
WSARecv( 이곳에서 나는 오류만 잡으면 성공

클라이언트에서
cin >> ip를 사용하면 개행 문자가 입력 버퍼에 남아있게 됩니다. 그 후 std::getline(std::cin, message);를 사용하면 남아있는 개행 문자가 getline()에 의해 즉시 소비되어 아무런 입력을 받지 못하게 됩니다.
