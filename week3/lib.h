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

// 고정 크기 메모리 할당을 위한 pointer 메모리 풀
class MemoryPool {
	size_t blockSize;
	vector<char*> freeBlocks; // 고정 블록을 가리키는 포인터 벡터
	mutex mtx;

public:
	// explicit: 묵시적 변환을 막음 == 반드시 생성자 호출을 통해서만 객체 생성 가능
	explicit MemoryPool(size_t blockSize, size_t reserve = 0) : blockSize(blockSize) {
		freeBlocks.reserve(reserve); // 임시 크기를 미리 할당, 속도 UP, 메모리 효율 DOWN

		for (size_t i = 0; i < reserve; i++) {
			freeBlocks.push_back(new char[blockSize]); // 고정 크기의 블록을 생성하여 벡터에 추가
		}
	}

	~MemoryPool() {
		for (char* block : freeBlocks) {
			delete[] block; // 벡터에 추가된 블록을 모두 삭제
		}
	}

	void* alloc() {
		// 뮤텍스를 이용하여 동시성 문제 해결
		lock_guard<mutex> lock(mtx);

		void* block;

		if (!freeBlocks.empty()) { // 블록이 남아있으면
			block = freeBlocks.back(); // 가장 최근에 추가된 블록을 가리키는 포인터를 반환
			freeBlocks.pop_back(); // 블록은 벡터에서 제거
		}
		else {
			block = new char[blockSize]; // 블록이 없으면 새로 생성
		}

		return block; // 블록을 가리키는 포인터(+블록) 반환
	}

	void dealloc(void* ptr) {
		// 뮤텍스를 이용하여 동시성 문제 해결
		lock_guard<mutex> lock(mtx);

		if (ptr != nullptr) {
			// 블록을 가리키는 포인터를 벡터에 추가
			// (외부에서 void*로 넘어온 포인터를 내부에서 char*로 캐스팅)
			freeBlocks.push_back(reinterpret_cast<char*>(ptr));
		}
	}

	void resize(size_t addreserve) {
		// 뮤텍스를 이용하여 동시성 문제 해결
		lock_guard<mutex> lock(mtx);//락가드

		// 벡터의 크기를 늘리고, 블록을 추가로 생성하여 벡터에 추가
		freeBlocks.reserve(freeBlocks.capacity() + addreserve);
		for (size_t i = 0; i < addreserve; i++) {
			freeBlocks.push_back(new char[blockSize]); // 고정 크기의 블록을 생성하여 벡터에 추가
		}
	}
};
//템플릿 프로그래밍은 어렵다. type...은 갯수에 상관없이 인자를 받을 수 있다.
template <typename T, typename... Args> // 가변인자 템플릿: 임의의 개수의 인자를 받을 수 있음
T* MemPool_new(MemoryPool& pool, Args&&... args) { // 완벽 전달: 인자를 그대로 전달
	T* block = reinterpret_cast<T*>(pool.alloc()); // void*를 할당 후 T*로 캐스팅  
	return new(block) T(forward<Args>(args)...); // 생성자 호출, block으로부터 new, 할당은 아니고 가져온것
	// forward<Args>(args)...: 완벽 전달된 인자를 생성자에 전달
}

template <typename T>//포인터에 참조 t, 포인터와 t가 다르다?
void MemPool_delete(MemoryPool& pool, T* ptr) {
	if (ptr) { // 포인터가 nullptr가 아니면
		ptr->~T(); // 소멸자 호출
		pool.dealloc(ptr); // 메모리 해제
	}
}

// 템플릿 기반의 객체 메모리 풀, 유연한 크기의 객체를 지원하지만 성능이 떨어짐
// template <typename T>
// class MemoryPool {
//     size_t blockSize;
// 	vector<T*> freeBlocks;

// public:
//     explicit MemoryPool(size_t reserve = 0) : blockSize(sizeof(T)) {
// 		for (size_t i = 0; i < reserve; i++) {
// 			freeBlocks.push_back(reinterpret_cast<T*>(new char[blockSize]));
// 		}
//     }

//     ~MemoryPool() {
//         for (auto block : freeBlocks) {
//             delete[] reinterpret_cast<char*>(block);
//         }
//     }

//     T* alloc() {
//         T* block;

//         if (!freeBlocks.empty()) {
//             block = freeBlocks.back();
//             freeBlocks.pop_back();
//         }
// 		else {
//             block = reinterpret_cast<T*>(new char[blockSize]);
//         }

//         return block;
//     }

//     void dealloc(T* ptr) {
//         if (ptr != nullptr) {
//             freeBlocks.push_back(ptr);
//         }
//     }
// };

// template <typename T, typename... Args>
// T* MemPool_new(MemoryPool<T>& pool, Args&&... args) {
//     T* block = pool.alloc();
//     return new(block) T(forward<Args>(args)...);
// }

// template <typename T>
// void MemPool_delete(MemoryPool<T>& pool, T* ptr) {
//     if (ptr) {
//         ptr->~T();
//         pool.dealloc(ptr);
//     }
// }
