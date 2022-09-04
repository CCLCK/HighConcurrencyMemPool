#define _CRT_SECURE_NO_WARNINGS 1

#include "CentralCache.h"
#include "PageCache.h"//��page cacheҪ

CentralCache CentralCache::_instance;


//��ȡһ���ǿյ�Span
Span* CentralCache::GetOneSpan(SpanList& list, size_t size)
{
	//����list���ҵ���һ���ǿյ�span
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
	//�Ҳ�������span�Ļ���ȥpage cacheҪ
	// �����ͼ�����page cache )
	//����NewSpan������page cache����ȡ
	list._mtx.unlock();
	PageCache::GetInstance()->_pageMtx.lock();
	Span* newSpan = PageCache::GetInstance()->NewSpan(SizeClass::NumMovePage(size));
	newSpan->_isUse = true;
	newSpan->_objSize = size;//������span���������ĵط�
	PageCache::GetInstance()->_pageMtx.unlock();

	//���ﻹ�����ż�������Ϊ��ʱ����̶߳����ò������newSpan,newSpan��һ���ֲ�����
	//�з�span
	//span��һ���ṹ�壬����Ҫ����ҳ�������ַ�����ж����ڴ�һ��������
	//����β�壬����������CPU�����ʸ���
	char* start = (char*)((newSpan->_pageId) << PAGE_SHIFT);
	size_t bytes = (newSpan->_n) << PAGE_SHIFT;//newSpan�Ĵ�С
	char* end = start + bytes;

	//Ȼ���newSpan��ǰ�漸���ֽ���ͷ
	newSpan->_freeList = start;//_freeListָ���Ҳ����Ч�ڴ��
	start += size;
	char* tail = (char*)(newSpan->_freeList);
	while (start < end)
	{
		NextObj(tail) = start;
		tail = (char*)NextObj(tail);
		start += size;
	}
	//������tail����һ��Ϊ�գ���Ȼ������
	NextObj(tail) = nullptr;
	//�кú�����������span�ŵ�Ͱ��ȥ������pageCache��������
	list._mtx.lock();
	list.PushFront(newSpan);
	return newSpan;
}

//�����Ļ����ȡһ�������Ķ����thread cache
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t batchNum, size_t size)
{
	//����size���Ͱ��Ȼ�����Ͱ��
	size_t index = SizeClass::Index(size);
	_spanList[index]._mtx.lock();
	//����GetOneSpan�õ�һ����Ч��Span��ȥspanList����ȥ�ң��Ҳ�����ȥpage cache)
	Span* span = GetOneSpan(_spanList[index], size);
	//����һ��span�Ͷ�Ӧ��List
	assert(span && span->_freeList);
	//��span�л�ȡbatchNum������
	//�Ӷ�Ӧ��List�����ó�batchNum����
	//���ÿ���batchNum>List������ŵ��ڴ���Ŀ������ֵ��ʵ�ʻ�ȡ������Ŀ
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

	//ע�����ĩβָ����ÿ�,��֤�ó�����start��end����
	span->_freeList = NextObj(end);
	NextObj(end) = nullptr;
	span->_useCount += actualNum;//��actualNum���ֳ�ȥ��
	//����
	_spanList[index]._mtx.unlock();

	//returnʵ�ʵ���Ŀ
	return actualNum;
}

//start�����С���ڴ����ʼ��ַ��������start��ʼ��������С���ڴ���ܲ�������һ��span
//������Ҫ��ÿһ��С���ڴ浥������
void CentralCache::ReleaseListToSpans(void* start, size_t size)
{
	//�ȿ�startָ����ڴ������ĸ�Ͱ��Ȼ����Ͱ��
	size_t index = SizeClass::Index(size);

	_spanList[index]._mtx.lock();
	while (start)
	{
		void* next = NextObj(start);
		//�����ڴ���ַ����֪���Լ��ǵڼ�ҳ��span���¼��ҳ��
		//�������Ǽ�¼ҳ��id��span��ӳ�䣬���ݵ�ַ���ҳ�ź�ֱ��ӳ�䵽��Ӧ��span
		//ӳ���ϵ����������pageCache�һ��span����ӳ����ҳ��
		
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);//Map�ڲ��Ѿ����������������Ż���
		//�ҵ���Ӧ��span���start��Ӧ��С���ڴ�ͷ���central cache��Ӧ��span���������
		//ͬʱusecount--������0˵���г�ȥ������С���ڴ涼������
		NextObj(start) = span->_freeList;
		span->_freeList = start;
		span->_useCount--;

		//����0˵�����span���Ի���page cache��page cache�ٳ���ǰ��ҳ�úϲ�
		//����page cache:������������ָ���ÿ�
		//����pagecacheǰ�ǵý����ͼӴ��������ﴦ����ǰ���actualNum
		if (span->_useCount==0)//С���ڴ涼������
		{
			//���span���Ի��ո�page cache����ǰ��ҳ�ĺϲ�
			//central cache���õ����span,����page cache��ReleaseSpanToPageCache
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
	//����
	_spanList[index]._mtx.unlock();

}
