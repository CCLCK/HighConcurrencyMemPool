#pragma once

#include "Common.h"

//һ��������Ķ���߳̿�������ͬһ��central cache����Ȼ����ģʽ
class CentralCache
{
public:
	//����
	static CentralCache* GetInstance()
	{
		return &_instance;
	}

	//��ȡһ���ǿյ�Span
	Span* GetOneSpan(SpanList& list, size_t size);

	//�����Ļ����ȡһ�������Ķ����thread cache
	 size_t FetchRangeObj(void*& start, void*& end, size_t n, size_t size);

	 void ReleaseListToSpans(void* start,size_t size);

private:
	//ӳ������thread cacheһ��������ȻͰ������һ��
	SpanList _spanList[NFREELIST];

	//����ģʽ
	//���캯��˽�У�ɾ����������͸�ֵ����
	CentralCache() {}
	CentralCache(const CentralCache&) = delete;
	CentralCache& operator=(const CentralCache&) = delete;
	// ��̬����cpp�ļ����ʼ����
	static CentralCache _instance;
	
};
