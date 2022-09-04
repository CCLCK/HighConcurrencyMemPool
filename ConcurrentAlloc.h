#pragma once

#include "Common.h"
#include "ThreadCache.h"
#include "PageCache.h"
#include "ObjectPool.h"
static void* ConcurrentAlloc(size_t size)//size��������ֽ�
{
	if (size>MAX_BYTES)//����256K����page cache
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
	//����TLS
	if (pTLSThreadCache==nullptr)
	{
		pTLSThreadCache = new ThreadCache;
	}
	return pTLSThreadCache->Allocate(size);
}

static void ConcurrentFree(void* ptr)
{
	//����ָ���size
	assert(ptr);
	//����128ҳ�ĸ��ݵ�ַ�õ���Ӧ��span�ٻ���
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

//�Ż���size
//1. ӳ��ҳ�ź�size
//2.��span����һ�����ԣ�����objectSize����ʾ�кõ�С����Ĵ�С
//span�Ǵ�page cache�ó���Ȼ���еģ��е�ʱ�򱣴�һ�£���Ȼ�����ͷŵ�ʱ���ҵ�����ڴ������ĸ�span����Ȼ��֪�����span�Ƕ��
//�������256Kb����Ҫ���⴦��
//��������µ����⣬�����ڷ���map����Ҫ��������ʹ��mapʱ��������unique_lock��RAII���������������˺���Ļ������Ż� 
//������ڽӿ����������page cache�������׵�������

//�ҵ�ӳ���Ͱ���������

