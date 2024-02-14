#pragma once

#include <iostream>

// 거의 사용되지 않는 내용을 Windows 헤더에서 제외
#define WIN32_LEAN_AND_MEAN 
#include <Windows.h>

#include <WinSock2.h>
#include <mswSock.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#include <fcntl.h>
#include <cstdint>
#include <cassert>
#include <vector>
#include <atomic>
#include <thread>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <list>
#include <queue>
#include <stack>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <functional>

using namespace std;

#define OUT

#define ASSERT_CRASH(expr) { \
	if (!(expr)) { \
		__analysis_assume(expr); \
	} \
}