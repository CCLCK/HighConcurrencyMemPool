#pragma once

#include "Common.h"


class PageCache
{
public:
	//����
	static PageCache* GetInstance()
	{
		return &_instance;
	}

	Span* NewSpan(size_t k);
	//����
	std::mutex _pageMtx;
private:
	//����ģʽ
	PageCache() {};
	PageCache(PageCache&) = delete;
	PageCache& operator=(const PageCache&) = delete;
	static PageCache _instance;
	//spanList
	SpanList _spanList[NPAGES];
	


};

