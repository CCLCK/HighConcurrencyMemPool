#pragma once

#include "Common.h"
#include "ObjectPool.h"
#include "PageMap.h"
class PageCache
{
public:
	//����
	static PageCache* GetInstance()
	{
		return &_instance;
	}

	Span* NewSpan(size_t k);

	Span* MapObjectToSpan(void* obj);//��һ������͸����Ӧ��span

	void ReleaseSpanToPageCache(Span* span);

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
	
	//ҳ�ź�span��ӳ���ϵ
	//std::unordered_map<PAGEID, Span*> _idSpanMap;
#ifdef _WIN64
	TCMalloc_PageMap3<64 - PAGE_SHIFT> _idSpanMap;
#elif _WIN32
	TCMalloc_PageMap1<32 - PAGE_SHIFT> _idSpanMap;
#else
	//linux

#endif // _WIN64

	

	//������滻new
	ObjectPool<Span> _spanPool;
};

