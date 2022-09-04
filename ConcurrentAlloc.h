#pragma once

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"
static void* ConcurrentAlloc(size_t size)//size是申请的字节
{
	if (size>MAX_BYTES)//大于256K的找page cache
	{
		size_t alignSize = SizeClass::RoundUp(size);
		size_t kpage = alignSize >> PAGE_SHIFT;
		PageCache::GetInstance()->_pageMtx.lock();
		Span* span = PageCache::GetInstance()->NewSpan(kpage);
		span->_objSize = size;
		span->_isUse = true;
		PageCache::GetInstance()->_pageMtx.unlock();
		void* ptr = (void*)((span->_pageId) << PAGE_SHIFT);
		return ptr;
	}
	//调用TLS
	if (pTLSThreadCache==nullptr)
	{
		pTLSThreadCache = new ThreadCache;
	}
	return pTLSThreadCache->Allocate(size);
}

static void ConcurrentFree(void* ptr)
{
	//断言指针和size
	assert(ptr);
	//大于128页的根据地址拿到对应的span再回收
	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objSize;
	if (size>MAX_BYTES)
	{
		PageCache::GetInstance()->_pageMtx.lock();
		PageCache::GetInstance()->ReleaseSpanToPageCache(span);
		PageCache::GetInstance()->_pageMtx.unlock();
	}
	else
	{
		assert(pTLSThreadCache);
		pTLSThreadCache->Deallocate(ptr,size);
	}
}

//优化掉size
//1. 映射页号和size
//2.在span里多加一个属性，叫做objectSize，表示切好的小对象的大小
//span是从page cache拿出来然后切的，切的时候保存一下，自然我们释放的时候找到这个内存属于哪个span，自然就知道这个span是多大
//如果大于256Kb的需要特殊处理
//这里出现新的问题，到处在访问map，需要加锁（在使用map时加锁，用unique_lock，RAII的锁），所以有了后面的基数树优化 
//如果不在接口里加锁，用page cache的锁容易导致死锁

//找到映射的桶，插入对象

