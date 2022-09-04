#pragma once

#include "Common.h"

class ThreadCache
{
public:
	//申请和释放对象，每个线程借助TLS去调用Allocate申请内存
	//每个线程调用Deallocate释放内存
	void* Allocate(size_t size);

	void Deallocate(void* ptr, size_t size);

	//从中心缓存获取对象(当thread cache不够时）
	//Fetch(获取)，参数size是要的大小
	//参数index就是size对应的桶号，传入是因为线程缓存和中心缓存映射规则相同
	void* FetchFromCentralCache(size_t index, size_t size);

	//释放对象链表过长时，回收内存到中心缓存
	void ListTooLong(FreeList& list, size_t size);

private:
	FreeList _freeList[NFREELIST];
};

//每个线程都有一个，避免了加锁
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
//static保证只在当前文件可见
  