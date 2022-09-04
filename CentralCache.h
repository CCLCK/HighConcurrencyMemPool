#pragma once

#include "Common.h"

//一个进程里的多个线程看到的是同一个central cache，显然单例模式
class CentralCache
{
public:
	//单例
	static CentralCache* GetInstance()
	{
		return &_instance;
	}

	//获取一个非空的Span
	Span* GetOneSpan(SpanList& list, size_t size);

	//从中心缓存获取一定数量的对象给thread cache
	 size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t size);

	 void ReleaseListToSpans(void* start,size_t size);

private:
	//映射规则和thread cache一样，那自然桶的数量一样
	SpanList _spanList[NFREELIST];

	//饿汉模式
	//构造函数私有，删除拷贝构造和赋值重载
	CentralCache() {}
	CentralCache(const CentralCache&) = delete;
	CentralCache& operator=(const CentralCache&) = delete;
	// 静态（在cpp文件里初始化）
	static CentralCache _instance;
	
};
