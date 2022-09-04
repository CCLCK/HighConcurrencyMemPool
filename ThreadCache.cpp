#define _CRT_SECURE_NO_WARNINGS 1

#include "ThreadCache.h"
#include "CentralCache.h"

void* ThreadCache::Allocate(size_t size)
{
	//assert断言申请的内存小于256KB
	assert(size < 256 * 1024);
	//拿到size对应的向上对齐数
	size_t alignSize = SizeClass::RoundUp(size);
	//根据对齐数拿到相对应的桶
	size_t index = SizeClass::Index(size);
	//去桶里要内存，没有就去下一层要，有的话直接拿一个
	if (_freeList[index].Empty())
	{
		//为空，去找central cache
		return FetchFromCentralCache(index, alignSize);
	}
	else
	{
		return _freeList[index].Pop();
	}
}


void ThreadCache::Deallocate(void* ptr, size_t size)
{
	//断言ptr不为空且size小于256KB
	assert(ptr);
	assert(size<256*1024);
	//算出桶号，把相应的块头插入桶中即可
	size_t index = SizeClass::Index(size);
	_freeList[index].Push(ptr);
	//链表太长超过一次批量就开始回收
	if (_freeList[index].Size()>=_freeList[index]._MaxSize)
	{
		ListTooLong(_freeList[index], size);
	}
}

void* ThreadCache::FetchFromCentralCache(size_t index, size_t size)
{
	//慢开始调节算法
	//原因：重复的去向central cache要会增加锁的竞争进而影响效率
	//为了进一步提升性能，采用申请一次给多个，那每次申请给十个可能有点浪费，所以就有了慢开始算法
	//1. 最开始不会一次给多个，因为太多了用不完
	//2.如果不要这个size，batchNum就会往上涨直到上限
	//3.size越大给的越少，size越小给的越多  
	int num = SizeClass::NumMoveSize(size);//这里传入的size已经是对齐的了
	
	//maxSize和慢开始算法决定central cache给thread cache多少个
	//如果给的是maxSize，就maxSize+=1,然后慢开始得到的batchNum决定了要的上限
	int batchNum = (num > _freeList[index]._MaxSize) ? _freeList[index]._MaxSize : num;
	if (batchNum == _freeList[index]._MaxSize)
	{
		_freeList[index]._MaxSize+=1;//thread cache到central cache时maxsize++
	}

	//从central cache的某个span里拿batchNum个size大小的对象出来给thread cache
	//没有batchNum个就有多少给多少，即actualNum
	//断言保证至少有一个
	assert(batchNum > 0);
	void* start = nullptr;
	void* end = nullptr;
	size_t actualNum=CentralCache::GetInstance()->FetchRangeObj(start, end, batchNum, size);
	assert(actualNum > 0);
	//如果只有一个，断言确定一下
	if (actualNum==1)
	{
		assert(start == end);
		//_freeList[index].Push(start);
	}
	else
	{
		//如果不止一个就得把这一段范围的插入thread cache，这样thread cache的桶里就有可使用的内存块了
		//start是要用的，后面的才是我们用的有效结点
		_freeList[index].PushRange(NextObj(start), end,actualNum-1);
	}

	return start;//返回拿到的内存块的起始地址
}

void ThreadCache::ListTooLong(FreeList& list, size_t size)
{
	//太长了，拿走头上的一个批量(MaxSize)，即PopRange一批
	void* start = nullptr;
	void* end = nullptr;
	list.PopRange(start, end, list._MaxSize);
	//拿出来后再把这一段还给span
	CentralCache::GetInstance()->ReleaseListToSpans(start, size);
}

//<=256KB 三层缓存
//32-128page 找page cache
//>128page 找系统 ConcurrentAlloc,RoundUp,NewSpan，ConcurrentFree
//ConcurrentAlloc算出页号调用NewSpan
//RealeaseSpanToPageCache
//替换new,类里整一个池子，然后池子去new替代原生new,同理替换原生delete
