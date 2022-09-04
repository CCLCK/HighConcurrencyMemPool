#pragma once

#include <iostream>
#include <assert.h>
#include <thread>
#include<mutex>
#include <unordered_map>
using std::cout;
using std::endl;

static const size_t NFREELIST = 208;//256KB按我们的映射一共有208个桶
static const size_t MAX_BYTES = 256 * 1024;
static const size_t NPAGES = 129;//pagecache里最大管理多少页的span
static const size_t PAGE_SHIFT = 13;



//PAHE_SHIFT便于计算一次要的页数

//条件编译保证能够存储页号不溢出
//64位下会定义_WIN64和_WIN32两个宏，32位只会定义_WIN32
#ifdef _WIN64
typedef unsigned long long PAGEID;
#elif _WIN32
typedef size_t PAGEID;
#else
typedef unsigned long long PAGEID;
#endif // _WIN64


#ifdef _WIN32
#include<windows.h>
#else 
//linux ...
#endif

// 直接去堆上按页申请空间
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux下brk mmap等
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

// 直接把这块空间还给堆
inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap等
#endif
}



static void*& NextObj(void* obj)
{
	return *((void**)obj);
}

//管理切分好的小对象的自由链表
class FreeList
{
public:
	void Push(void* obj)
	{
		/*if (obj==nullptr)
		{
			assert(false);
		}*/
		assert(obj);
		NextObj(obj) = _freeList;
		_freeList = obj;
		_size++;
	}
	void PushRange(void* start, void* end,size_t n)//end指向最后一个有效的内存块，而不是nullptr
	{
		NextObj(end) = _freeList;
		_freeList = start;
		_size += n;
	}
	void* Pop()
	{
		/*if (_freeList==nullptr)
		{
			assert(false);
		}*/
		assert(_freeList);
		void* obj = _freeList;
		_freeList = NextObj(_freeList);
		_size--;
		return obj;//头删结点的指针
	}
	bool Empty()
	{
		return _freeList == nullptr;
	}
	size_t Size()
	{
		return _size;
	}
	void PopRange(void*& start, void*& end, size_t n)
	{
		assert(n <= _size);
		start = _freeList;
		end = _freeList;
		for (size_t i=0;i<n;i++)
		{
			end = NextObj(end);
		}
		_freeList = end;
		end = nullptr;
		_size -= n;
	}
	size_t _MaxSize = 1;

private:
	void* _freeList=nullptr;
	size_t _size = 0;
};

//计算对象大小的映射规则  
class SizeClass
{
public:
	// 整体控制在最多10%左右的内碎片浪费
	// [1,128]					8byte对齐	    freelist[0,16)
	// [128+1,1024]				16byte对齐	    freelist[16,72)
	// [1024+1,8*1024]			128byte对齐	    freelist[72,128)
	// [8*1024+1,64*1024]		1024byte对齐     freelist[128,184)
	// [64*1024+1,256*1024]		8*1024byte对齐   freelist[184,208)
	static inline size_t  _RoundUp(size_t bytes,size_t alignNum)//向上对齐
	{
		//bytes是申请的字节，alignNum是对齐数
		//bytes为6就给8字节，bytes为129就给144字节
		return (((bytes)+alignNum - 1) & ~(alignNum - 1));
		//~(alignNum-1）让alignNum为1的那一位后面的位都是0，其余的位都是1，保证答案肯定是对齐数的倍数
		//((bytes)+alignNum - 1)让某位变成1，保证答案一定向上对齐
		
		//上面两个部分&一下，得到的数后面几位都是0，其余的位和((bytes)+alignNum - 1)的每一位对应相等
		//后面几位都是0保证得到的数一定是对齐数的倍数
		//自己代两个数算一下就明白了
	}
	static inline size_t RoundUp(size_t size)
	{
		assert(size > 0);
		if (size<=128)
		{
			return _RoundUp(size, 8);
		}
		else if (size <= 1024)
		{
			return _RoundUp(size, 16);
		}
		else if (size <= 8*1024)
		{
			return _RoundUp(size, 128);
		}
		else if (size <= 64*1024)
		{
			return _RoundUp(size, 1024);
		}
		else if (size <= 256*1024)
		{
			return _RoundUp(size, 8*1024);
		}
		else
		{
			//申请大于256KB的
			//assert(false);

			//以1<<PAGE_SHIFT对齐
			return _RoundUp(size, 1 << PAGE_SHIFT);
		}
	}

	//bytes大小不同，我们是对其分段处理的
	//比如1-128是第一段，这段总共有128/8=16个桶
	//128-1024是第二段，一共有（1024-128)/16=56个桶
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		//bytes是要申请的字节数，align_shift是和对齐数对应的
		//比如对齐数是8时align_shift传3，是16就传4,align=2^align_shift
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
		//(bytes + (1 << align_shift) - 1)超过当前的bytes向上对齐的对齐数，小于下一个对齐数
		//比如bytes是7，那 8<(bytes + (1 << align_shift) - 1)<16
		//bytes + (1 << align_shift) - 1)再右移align_shift这么多位，相当于除以alignNum
		//得到一个答案就是在这一段里的第几个桶
	}

	// 计算映射的哪一个自由链表桶
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);

		// 每个区间有多少个链
		static int group_array[4] = { 16, 56, 56, 56 };
		if (bytes <= 128) {
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024) {
			//_Index(bytes - 128, 4)拿到当前bytes属于这段的第几个桶，再加上前面有的桶，就知道bytes应该映射到哪个桶了
			return _Index(bytes - 128, 4) + group_array[0];
		}
		else if (bytes <= 8 * 1024) {
			return _Index(bytes - 1024, 7) + group_array[1] + group_array[0];
		}
		else if (bytes <= 64 * 1024) {
			return _Index(bytes - 8 * 1024, 10) + group_array[2] + group_array[1] + group_array[0];
		}
		else if (bytes <= 256 * 1024) {
			return _Index(bytes - 64 * 1024, 13) + group_array[3] + group_array[2] + group_array[1] + group_array[0];
		}
		else {
			assert(false);

			
		}

		return -1;
	}

	//慢开始算法
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);

		size_t num = MAX_BYTES / size;//MAX_BYTES就是256KB
		if (num<2)
		{
			num = 2;
		}
		if (num>512)
		{
			num = 512;
		}
		return num;
	}

	//NumMovePage 一次要多少页
	static size_t NumMovePage(size_t size)//size这么大小的对象一次给几页
	{
		size_t num = NumMoveSize(size);//要几个
		size_t npage = num * size;//一共要这么多个字节
		npage >>= PAGE_SHIFT;//要多少页
		if (npage==0)
		{
			npage = 1;
		}
		return npage;
	}
private:
};


//管理多个连续页的大块内存跨度结构
//属性记得初始化
struct Span
{
	//大块内存起始页的页号
	//64位下以每页8KB为例一共有2^64/2^13=2^51页，int存不下，用unsigned long long+条件编译解决
	PAGEID _pageId = 0;
	//页的数量,有几页大小
	size_t _n = 0;
	//双向链表的结构
	Span* _next=nullptr;
	Span* _prev = nullptr;

	//切好的小块内存被分配给thread cache的计数
	size_t _useCount = 0;
	//切好的小块内存的自由链表
	void* _freeList = nullptr;
	//当前span是否在被使用
	bool _isUse = false;

	//这个span切的小对象的大小
	size_t _objSize = 0;
};

//带头双向循环链表，把Span串起来
class SpanList
{

public:
	//双向循环链表初始化
	SpanList()
	{
		_head = new Span;//不new _head会是空
		_head->_next = _head;
		_head->_prev = _head;
	}

	void Insert(Span* pos, Span* newSpan)
	{
		//在pos位置插入newSpan
		//断言pos和newSpan
		assert(pos && newSpan);
		//开始插入
		Span* prev = pos->_prev;
		prev->_next = newSpan;
		newSpan->_next = pos;
		pos->_prev = newSpan;
		newSpan->_prev = prev;
	}

	void Erase(Span* pos)//这里并不delete,delete就还给系统了
	{
		//删掉pos位置的span
		//断言span和哨兵位的头结点
		assert(pos&&pos!=_head);
		//开始删除
		Span* prev = pos->_prev;
		Span* next = pos->_next;
		prev->_next = next;
		next->_prev = prev;
	}

	//提供一个begin和end的接口，注意双链表是带头的
	Span* Begin()
	{
		return _head->_next;
	}
	Span* End()
	{
		return _head;
	}

	bool Empty()
	{
		return _head->_next == _head;
	}
	Span* PopFront()
	{
		//保证有元素再头删
		
		/*if (Empty())
		{
			int x = 0;
		}*/
		assert(!Empty());
		Span* front = _head->_next;
		Erase(front);
		return front;
	}
	void PushFront(Span* span)
	{
		Insert(Begin(), span);
	}
private:
	Span* _head;

public:
	//桶锁
	std::mutex _mtx;
};
