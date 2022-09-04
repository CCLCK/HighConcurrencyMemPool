 #define _CRT_SECURE_NO_WARNINGS 1

#include "PageCache.h"

PageCache PageCache::_instance;
//��ȡһ��Kҳ��span��central cache
Span* PageCache::NewSpan(size_t k)
{
	//assert��֤k����0С������ҳ
	//assert(k > 0 && k < NPAGES);
	assert(k > 0 );

	//����128ҳ�ĵ�ֱ��ȥ�Ҷ�Ҫ
	//����һ��ר����Span�����������ڴ棬������Ӧӳ��
	if (k>=NPAGES)
	{
		void* ptr = SystemAlloc(k);
		Span* span = _spanPool.New();
		span->_n = k;
		span->_pageId = ((PAGEID)ptr) >> PAGE_SHIFT;
		//����ӳ��
		_idSpanMap.set(span->_pageId, span);
		return span;
	}

	//1. ���õݹ�������Ϊ�����ݹ�����Լ�
	//2. �����Ӻ��������������Ӻ���
	//������õ�һ�֣�ͬʱ�ð�central cache�������ˣ��ɽ�һ�����Ч�� 
	//���Ļ���������������������central cache����NewSpanʱ�����������ͱ���������
	//ͬʱҲ������central cache��ȡ�����
	 
	 
	//��ȥ����k��Ͱ����û��span
	if (!_spanList[k].Empty())
	{
		Span* kSpan = _spanList[k].PopFront();
		for (PAGEID i = 0; i < kSpan->_n; i++)
		{
			//_idSpanMap[kSpan->_pageId + i] = kSpan;
			_idSpanMap.set(kSpan->_pageId + i, kSpan);
		}
		return kSpan;
	}
	//�ߵ���˵��û�У�ȥ�Һ����Ͱ���������Ͱ��û��
	//�еĻ����з֣�û�еĻ�׼����ϵͳ����
	//�з֣���ͷ���� �õ�K��n-k��span,kҳ�ĸ�central cache,n-k�Ĺҵ�n-k��Ӧ��Ͱ
	//�з�span�ı����ǹ���span�����ݽṹ����ʼҳ��ַ��span��С)
	for (int i=k+1;i<NPAGES;i++)
	{
		if (!_spanList[i].Empty())
		{
			Span* nSpan = _spanList[i].PopFront();
			//Span* kSpan =new Span;
			Span* kSpan = _spanPool.New();
			kSpan->_n = k;
			kSpan->_pageId = nSpan->_pageId;
			//��ʼ�з�
			nSpan->_n -= k;
			nSpan->_pageId +=k;//�Լ���ҳ��������kҳ
			
			//�����һ�ȥ
			_spanList[nSpan->_n].PushFront(nSpan);

			//�洢nSpan����βҳ������page cache����
			//nSpan����û�����ã�ӳ����βҳ������ռ���
			//_idSpanMap[nSpan->_pageId] = nSpan;
			//_idSpanMap[nSpan->_pageId + nSpan->_n - 1] = nSpan;//ע����n-1
			_idSpanMap.set(nSpan->_pageId, nSpan);
			_idSpanMap.set(nSpan->_pageId + nSpan->_n - 1, nSpan);
			//�����kSpan����ӳ�䷽��central cache����С�ڴ�
			for (PAGEID i=0;i<kSpan->_n;i++)
			{
				//_idSpanMap[kSpan->_pageId + i] = kSpan;
				_idSpanMap.set(kSpan->_pageId + i, kSpan);

			}
			return kSpan;
		}
	}
	//����˵��pageCache�Ҳ�������Ҫ����ˣ���ȥ��ϵͳҪ
	//��ϵͳҪ�Ĵ�ҳspan���ص���һ����ַ
	//�ѵ�ַת��ҳ�ţ�����8K
	//�õ���ҳ����Ҫ�з֣��ݹ�����Լ������ٴ��븴��  

	//��ʼ����
	//Span* bigSpan = new Span;
	Span* bigSpan = _spanPool.New();

	void* ptr=SystemAlloc(NPAGES - 1);
	bigSpan->_n = NPAGES - 1;
	bigSpan->_pageId = ((PAGEID)ptr) >> PAGE_SHIFT;
	
	//�����������
	_spanList[bigSpan->_n].PushFront(bigSpan);
	return NewSpan(k);
}


Span* PageCache::MapObjectToSpan(void* obj)//��һ������͸����Ӧ��span
{
	//���ݵ�ַ���ҳ�ţ�����8k)
	PAGEID id = (PAGEID)obj >> PAGE_SHIFT;
	//ȥ��ϣ����ȥ�����ҳ�ţ����ܲ����ҵ����span
	//std::unique_lock<std::mutex>lock(_pageMtx);//RAII����,����ƿ��
	//auto ret = _idSpanMap.find(id);
	//if (ret!=_idSpanMap.end())
	//{
	//	return ret->second;
	//}
	//else
	//{
	//	//�Ҳ���˵���������ˣ�˵��ָ�������ڴ�û�н���ӳ�䣬assert
	//	assert(false);
	//}

	auto ret = (Span*)_idSpanMap.get(id);
	assert(ret != nullptr);
	return ret;
}


void PageCache::ReleaseSpanToPageCache(Span* span)
{
	//span̫����ֱ�ӻ���ϵͳ
	if (span->_n>=NPAGES)
	{
		void* ptr =(void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		//delete span;//���span��new������
		_spanPool.Delete(span);
		return;
	}
	//��central cache�������ĺϲ�
	//��spanǰ���ҳ���Ժϲ��������ڴ���Ƭ����
	//��һ����־_isUse��־�����û�����ã���ֹcentral cache�����зֵı��ù���������
	//������ʹ�õ�span�ı�־�ĳ�true
	//�洢spanǰ��ҳ��ӳ�䣬Ȼ��ȥ��ǰ��ҳ�����ܲ����ҵ�
	//����span��_isUse��ҳ�ŵ�ӳ���Լ��ϲ���span�Ĵ�С������ж�


	//��ʼ��ǰ�ϲ�
	//�ı�span������
	while (1)
	{
		//�ҵ�ǰ���ҳ
		PAGEID prevId = span->_pageId - 1;

		//û��ǰ���ҳ
		//auto ret = _idSpanMap.find(prevId);
		auto ret = (Span*)_idSpanMap.get(prevId);
		//if (ret==_idSpanMap.end())
		if (ret==nullptr)
		{
			break;
		}
		//ǰ���ҳ��ʹ����
		Span* prevSpan = ret;
		if (prevSpan->_isUse==true)
		{
			break;
		}
		//�ϲ����ҳ���ڵ���NPAGES
		if (prevSpan->_n+span->_n>=NPAGES)
		{
			break;
		}

		//���Ժϲ���
		span->_pageId = prevSpan->_pageId;
		span->_n += prevSpan->_n;
		_spanList[prevSpan->_n].Erase(prevSpan);//���span��prev��nextָ��
		//����ǰ����span�Ľṹ��delete�ˣ������ϲ���һ���϶���һ��û��
		//delete prevSpan;
		_spanPool.Delete(prevSpan);

	}
	

	//�ٽ������ϲ�
	while (1)
	{
		//�ҵ������ҳ
		PAGEID nextId = span->_pageId+span->_n ;

		//û�к����ҳ
		//auto ret = _idSpanMap.find(nextId);
		auto ret = (Span*)_idSpanMap.get(nextId);
		if (ret == nullptr)
		{
			break;
		}
		//�����ҳ��ʹ����
		Span* nextSpan = ret;
		if (nextSpan->_isUse == true)
		{
			break;
		}
		//�ϲ����ҳ���ڵ���NPAGES
		if (nextSpan->_n + span->_n >= NPAGES)
		{
			break;
		}

		//���Ժϲ���
		span->_pageId = nextSpan->_pageId;
		span->_n += nextSpan->_n;
		_spanList[nextSpan->_n].Erase(nextSpan);//���span��prev��nextָ��
		//����ǰ����span�Ľṹ��delete�ˣ������ϲ���һ���϶���һ��û��
		//delete nextSpan;
		_spanPool.Delete(nextSpan);

	}

	//������ϲ��õ�span�һ�page cache
	_spanList[span->_n].PushFront(span);

	//�ϲ����������ͬʱ������βҳ��ӳ�䣬��_isUse��Ϊfalse
	/*_idSpanMap[span->_pageId] = span;
	_idSpanMap[span->_pageId+span->_n-1] = span;*/
	_idSpanMap.set(span->_pageId, span);
	_idSpanMap.set(span->_pageId + span->_n - 1, span);
	span->_isUse = false;

	//�ǵ�����

}


