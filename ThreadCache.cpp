#define _CRT_SECURE_NO_WARNINGS 1

#include "ThreadCache.h"
#include "CentralCache.h"

void* ThreadCache::Allocate(size_t size)
{
	//assert����������ڴ�С��256KB
	assert(size < 256 * 1024);
	//�õ�size��Ӧ�����϶�����
	size_t alignSize = SizeClass::RoundUp(size);
	//���ݶ������õ����Ӧ��Ͱ
	size_t index = SizeClass::Index(size);
	//ȥͰ��Ҫ�ڴ棬û�о�ȥ��һ��Ҫ���еĻ�ֱ����һ��
	if (_freeList[index].Empty())
	{
		//Ϊ�գ�ȥ��central cache
		return FetchFromCentralCache(index, alignSize);
	}
	else
	{
		return _freeList[index].Pop();
	}
}


void ThreadCache::Deallocate(void* ptr, size_t size)
{
	//����ptr��Ϊ����sizeС��256KB
	assert(ptr);
	assert(size<256*1024);
	//���Ͱ�ţ�����Ӧ�Ŀ�ͷ����Ͱ�м���
	size_t index = SizeClass::Index(size);
	_freeList[index].Push(ptr);
	//����̫������һ�������Ϳ�ʼ����
	if (_freeList[index].Size()>=_freeList[index]._MaxSize)
	{
		ListTooLong(_freeList[index], size);
	}
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	//����ʼ�����㷨
	//ԭ���ظ���ȥ��central cacheҪ���������ľ�������Ӱ��Ч��
	//Ϊ�˽�һ���������ܣ���������һ�θ��������ÿ�������ʮ�������е��˷ѣ����Ծ���������ʼ�㷨
	//1. �ʼ����һ�θ��������Ϊ̫�����ò���
	//2.�����Ҫ���size��batchNum�ͻ�������ֱ������
	//3.sizeԽ�����Խ�٣�sizeԽС����Խ��  
	int num = SizeClass::NumMoveSize(size);//���ﴫ���size�Ѿ��Ƕ������
	
	//maxSize������ʼ�㷨����central cache��thread cache���ٸ�
	//���������maxSize����maxSize+=1,Ȼ������ʼ�õ���batchNum������Ҫ������
	int batchNum = (num > _freeList[index]._MaxSize) ? _freeList[index]._MaxSize : num;
	if (batchNum == _freeList[index]._MaxSize)
	{
		_freeList[index]._MaxSize+=1;//thread cache��central cacheʱmaxsize++
	}

	//��central cache��ĳ��span����batchNum��size��С�Ķ��������thread cache
	//û��batchNum�����ж��ٸ����٣���actualNum
	//���Ա�֤������һ��
	assert(batchNum > 0);
	void* start = nullptr;
	void* end = nullptr;
	size_t actualNum=CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum > 0);
	//���ֻ��һ��������ȷ��һ��
	if (actualNum==1)
	{
		assert(start == end);
		//_freeList[index].Push(start);
	}
	else
	{
		//�����ֹһ���͵ð���һ�η�Χ�Ĳ���thread cache������thread cache��Ͱ����п�ʹ�õ��ڴ����
		//start��Ҫ�õģ�����Ĳ��������õ���Ч���
		_freeList[index].PushRange(NextObj(start), end,actualNum-1);
	}

	return start;//�����õ����ڴ�����ʼ��ַ
}

void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	//̫���ˣ�����ͷ�ϵ�һ������(MaxSize)����PopRangeһ��
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, list._MaxSize);
	//�ó������ٰ���һ�λ���span
	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}

//<=256KB ���㻺��
//32-128page ��page cache
//>128page ��ϵͳ ConcurrentAlloc,RoundUp,NewSpan��ConcurrentFree
//ConcurrentAlloc���ҳ�ŵ���NewSpan
//RealeaseSpanToPageCache
//�滻new,������һ�����ӣ�Ȼ�����ȥnew���ԭ��new,ͬ���滻ԭ��delete
