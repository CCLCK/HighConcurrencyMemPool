#pragma once

#include "Common.h"
#include "ObjectPool.h"
#include "PageMap.h"
class PageCache
{
public:
	//单例
	static PageCache* GetInstance()
	{
		return &_instance;
	}

	Span* NewSpan(size_t k);

	Span* MapObjectToSpan(void* obj);//给一个对象就给其对应的span

	void ReleaseSpanToPageCache(Span* span);

	//大锁
	std::mutex _pageMtx;
private:
	//单例模式
	PageCache() {};
	PageCache(PageCache&) = delete;
	PageCache& operator=(const PageCache&) = delete;
	static PageCache _instance;
	//spanList
	SpanList _spanList[NPAGES];
	
	//页号和span的映射关系
	//std::unordered_map<PAGEID, Span*> _idSpanMap;
#ifdef _WIN64
	TCMalloc_PageMap3<64 - PAGE_SHIFT> _idSpanMap;
#elif _WIN32
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;
#else
	//linux

#endif // _WIN64

	

	//对象池替换new
	ObjectPool<Span> _spanPool;
};

