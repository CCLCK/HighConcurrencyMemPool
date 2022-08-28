 #define _CRT_SECURE_NO_WARNINGS 1


#include "PageCache.h"

PageCache PageCache::_instance;
//��ȡһ��Kҳ��span��central cache
Span* PageCache::NewSpan(size_t k)
{
	//assert��֤k����0С������ҳ
	assert(k > 0 && k < NPAGES);
	//1. ���õݹ�������Ϊ�����ݹ�����Լ�
	//2. �����Ӻ��������������Ӻ���
	//������õ�һ�֣�ͬʱ�ð�central cache�������ˣ��ɽ�һ�����Ч�� 
	//���Ļ���������������������central cache����NewSpanʱ�����������ͱ���������
	//ͬʱҲ������central cache��ȡ�����
	 
	 
	//��ȥ����k��Ͱ����û��span
	if (!_spanList[k].Empty())
	{
		Span* kSpan = _spanList->PopFront();
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
			Span* kSpan =new Span;
			kSpan->_n = k;
			kSpan->_pageId = nSpan->_pageId;
			//��ʼ�з�
			nSpan->_n -= k;
			nSpan->_pageId +=k;//�Լ���ҳ��������kҳ
			
			//�����һ�ȥ
			_spanList[nSpan->_n].PushFront(nSpan);
			return kSpan;
		}
	}
	//����˵��pageCache�Ҳ�������Ҫ����ˣ���ȥ��ϵͳҪ
	//��ϵͳҪ�Ĵ�ҳspan���ص���һ����ַ
	//�ѵ�ַת��ҳ�ţ�����8K
	//�õ���ҳ����Ҫ�з֣��ݹ�����Լ������ٴ��븴��  

	//��ʼ����
	Span* bigSpan = new Span;
	void* ptr=SystemAlloc(NPAGES - 1);
	bigSpan->_n = NPAGES - 1;
	bigSpan->_pageId = ((PAGEID)ptr) >> PAGE_SHIFT;
	
	//�����������
	_spanList[bigSpan->_n].PushFront(bigSpan);
	return NewSpan(k);
}






