#define _CRT_SECURE_NO_WARNINGS 1

#include "CentralCache.h"
#include "PageCache.h"//找page cache要

CentralCache CentralCache::_instance;


//获取一个非空的Span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	//遍历list，找到第一个非空的span
	Span* cur = list.Begin();
	while (cur != list.End())
	{
		if (cur->_freeList != nullptr)
		{
			return cur;
		}
		else
		{
			cur = cur->_next;
		}
	}
	//找不到空闲span的话就去page cache要
	// 解锁和加锁（page cache )
	//调用NewSpan函数从page cache里面取
	list._mtx.unlock();
	PageCache::GetInstance()->_pageMtx.lock();
	Span* newSpan = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	newSpan->_isUse = true;
	newSpan->_objSize = size;//这里是span“出生”的地方
	PageCache::GetInstance()->_pageMtx.unlock();

	//这里还不急着加锁，因为此时别的线程都还拿不到这个newSpan,newSpan是一个局部变量
	//切分span
	//span是一个结构体，所以要根据页号算出地址，像切定长内存一样往后切
	//采用尾插，物理上连续CPU命中率更高
	char* start = (char*)((newSpan->_pageId) << PAGE_SHIFT);
	size_t bytes = (newSpan->_n) << PAGE_SHIFT;//newSpan的大小
	char* end = start + bytes;

	//然后把newSpan的前面几个字节作头
	newSpan->_freeList = start;//_freeList指向的也是有效内存块
	start += size;
	char* tail = (char*)(newSpan->_freeList);
	while (start < end)
	{
		NextObj(tail) = start;
		tail = (char*)NextObj(tail);
		start += size;
	}
	//别忘记tail的下一个为空，不然程序会崩
	NextObj(tail) = nullptr;
	//切好后加锁，把这个span放到桶里去，这样pageCache就能用了
	list._mtx.lock();
	list.PushFront(newSpan);
	return newSpan;
}

//从中心缓存获取一定数量的对象给thread cache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	//根据size算出桶，然后加上桶锁
	size_t index = SizeClass::Index(size);
	_spanList[index]._mtx.lock();
	//调用GetOneSpan拿到一个有效的Span（去spanList里面去找，找不到就去page cache)
	Span* span = GetOneSpan(_spanList[index], size);
	//断言一下span和对应的List
	assert(span && span->_freeList);
	//从span中获取batchNum个对象
	//从对应的List下面拿出batchNum个，
	//还得考虑batchNum>List下面挂着的内存数目，返回值是实际获取到的数目
	start = span->_freeList;
	end = start;
	int actualNum = 1;
	while (actualNum < batchNum && NextObj(end))
	{
		end = NextObj(end);
		actualNum++;
		if (actualNum == batchNum)
		{
			break;
		}
	}
	span->_useCount += actualNum;

	//注意对于末尾指针的置空,保证拿出来的start到end完整
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	span->_useCount += actualNum;//有actualNum个分出去了
	//解锁
	_spanList[index]._mtx.unlock();

	//return实际的数目
	return actualNum;
}

//start代表的小块内存的起始地址，但是以start开始的连续的小块内存可能并不属于一个span
//所以需要对每一个小块内存单独处理
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	//先看start指向的内存属于哪个桶，然后上桶锁
	size_t index = SizeClass::Index(size);

	_spanList[index]._mtx.lock();
	while (start)
	{
		void* next = NextObj(start);
		//根据内存块地址可以知道自己是第几页，span里记录了页号
		//所以我们记录页号id和span的映射，根据地址算出页号后直接映射到对应的span
		//映射关系的容器放在pageCache里，一个span可能映射多个页号
		
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);//Map内部已经上锁，后面会把锁优化掉
		//找到对应的span后把start对应的小块内存头插回central cache对应的span下面的链表
		//同时usecount--，减到0说明切出去的所有小块内存都回来了
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_useCount--;

		//减到0说明这个span可以还给page cache，page cache再尝试前后页好合并
		//还给page cache:把下面的链表和指针置空
		//调用pagecache前记得解锁和加大锁（这里处理下前面的actualNum
		if (span->_useCount==0)//小块内存都回来了
		{
			//这个span可以回收给page cache尝试前后页的合并
			//central cache里拿掉这个span,调用page cache的ReleaseSpanToPageCache
			_spanList[index].Erase(span);
			span->_prev = nullptr;
			span->_next = nullptr;
			span->_freeList = nullptr;
			_spanList[index]._mtx.unlock();

			PageCache::GetInstance()->_pageMtx.lock();
			PageCache::GetInstance()->ReleaseSpanToPageCache(span);
			PageCache::GetInstance()->_pageMtx.unlock();
			_spanList[index]._mtx.lock();

		}
		start = next;
	}
	//解锁
	_spanList[index]._mtx.unlock();

}
