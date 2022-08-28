#pragma once

#include "Common.h"


class PageCache
{
public:
	//单例
	static PageCache* GetInstance()
	{
		return &_instance;
	}

	Span* NewSpan(size_t k);
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
	


};

