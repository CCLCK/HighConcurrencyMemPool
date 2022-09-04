#pragma once

#include <iostream>
#include <assert.h>
#include <thread>
#include<mutex>
#include <unordered_map>
using std::cout;
using std::endl;

static const size_t NFREELIST = 208;//256KB�����ǵ�ӳ��һ����208��Ͱ
static const size_t MAX_BYTES = 256 * 1024;
static const size_t NPAGES = 129;//pagecache�����������ҳ��span
static const size_t PAGE_SHIFT = 13;



//PAHE_SHIFT���ڼ���һ��Ҫ��ҳ��

//�������뱣֤�ܹ��洢ҳ�Ų����
//64λ�»ᶨ��_WIN64��_WIN32�����꣬32λֻ�ᶨ��_WIN32
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

// ֱ��ȥ���ϰ�ҳ����ռ�
inline static void* SystemAlloc(size_t kpage)
{
#ifdef _WIN32
	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
	// linux��brk mmap��
#endif

	if (ptr == nullptr)
		throw std::bad_alloc();

	return ptr;
}

// ֱ�Ӱ����ռ仹����
inline static void SystemFree(void* ptr)
{
#ifdef _WIN32
	VirtualFree(ptr, 0, MEM_RELEASE);
#else
	// sbrk unmmap��
#endif
}



static void*& NextObj(void* obj)
{
	return *((void**)obj);
}

//�����зֺõ�С�������������
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
	void PushRange(void* start, void* end,size_t n)//endָ�����һ����Ч���ڴ�飬������nullptr
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
		return obj;//ͷɾ����ָ��
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

//��������С��ӳ�����  
class SizeClass
{
public:
	// ������������10%���ҵ�����Ƭ�˷�
	// [1,128]					8byte����	    freelist[0,16)
	// [128+1,1024]				16byte����	    freelist[16,72)
	// [1024+1,8*1024]			128byte����	    freelist[72,128)
	// [8*1024+1,64*1024]		1024byte����     freelist[128,184)
	// [64*1024+1,256*1024]		8*1024byte����   freelist[184,208)
	static inline size_t  _RoundUp(size_t bytes,size_t alignNum)//���϶���
	{
		//bytes��������ֽڣ�alignNum�Ƕ�����
		//bytesΪ6�͸�8�ֽڣ�bytesΪ129�͸�144�ֽ�
		return (((bytes)+alignNum - 1) & ~(alignNum - 1));
		//~(alignNum-1����alignNumΪ1����һλ�����λ����0�������λ����1����֤�𰸿϶��Ƕ������ı���
		//((bytes)+alignNum - 1)��ĳλ���1����֤��һ�����϶���
		
		//������������&һ�£��õ��������漸λ����0�������λ��((bytes)+alignNum - 1)��ÿһλ��Ӧ���
		//���漸λ����0��֤�õ�����һ���Ƕ������ı���
		//�Լ�����������һ�¾�������
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
			//�������256KB��
			//assert(false);

			//��1<<PAGE_SHIFT����
			return _RoundUp(size, 1 << PAGE_SHIFT);
		}
	}

	//bytes��С��ͬ�������Ƕ���ֶδ����
	//����1-128�ǵ�һ�Σ�����ܹ���128/8=16��Ͱ
	//128-1024�ǵڶ��Σ�һ���У�1024-128)/16=56��Ͱ
	static inline size_t _Index(size_t bytes, size_t align_shift)
	{
		//bytes��Ҫ������ֽ�����align_shift�ǺͶ�������Ӧ��
		//�����������8ʱalign_shift��3����16�ʹ�4,align=2^align_shift
		return ((bytes + (1 << align_shift) - 1) >> align_shift) - 1;
		//(bytes + (1 << align_shift) - 1)������ǰ��bytes���϶���Ķ�������С����һ��������
		//����bytes��7���� 8<(bytes + (1 << align_shift) - 1)<16
		//bytes + (1 << align_shift) - 1)������align_shift��ô��λ���൱�ڳ���alignNum
		//�õ�һ���𰸾�������һ����ĵڼ���Ͱ
	}

	// ����ӳ�����һ����������Ͱ
	static inline size_t Index(size_t bytes)
	{
		assert(bytes <= MAX_BYTES);

		// ÿ�������ж��ٸ���
		static int group_array[4] = { 16, 56, 56, 56 };
		if (bytes <= 128) {
			return _Index(bytes, 3);
		}
		else if (bytes <= 1024) {
			//_Index(bytes - 128, 4)�õ���ǰbytes������εĵڼ���Ͱ���ټ���ǰ���е�Ͱ����֪��bytesӦ��ӳ�䵽�ĸ�Ͱ��
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

	//����ʼ�㷨
	static size_t NumMoveSize(size_t size)
	{
		assert(size > 0);

		size_t num = MAX_BYTES / size;//MAX_BYTES����256KB
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

	//NumMovePage һ��Ҫ����ҳ
	static size_t NumMovePage(size_t size)//size��ô��С�Ķ���һ�θ���ҳ
	{
		size_t num = NumMoveSize(size);//Ҫ����
		size_t npage = num * size;//һ��Ҫ��ô����ֽ�
		npage >>= PAGE_SHIFT;//Ҫ����ҳ
		if (npage==0)
		{
			npage = 1;
		}
		return npage;
	}
private:
};


//����������ҳ�Ĵ���ڴ��Ƚṹ
//���Լǵó�ʼ��
struct Span
{
	//����ڴ���ʼҳ��ҳ��
	//64λ����ÿҳ8KBΪ��һ����2^64/2^13=2^51ҳ��int�治�£���unsigned long long+����������
	PAGEID _pageId = 0;
	//ҳ������,�м�ҳ��С
	size_t _n = 0;
	//˫������Ľṹ
	Span* _next=nullptr;
	Span* _prev = nullptr;

	//�кõ�С���ڴ汻�����thread cache�ļ���
	size_t _useCount = 0;
	//�кõ�С���ڴ����������
	void* _freeList = nullptr;
	//��ǰspan�Ƿ��ڱ�ʹ��
	bool _isUse = false;

	//���span�е�С����Ĵ�С
	size_t _objSize = 0;
};

//��ͷ˫��ѭ��������Span������
class SpanList
{

public:
	//˫��ѭ�������ʼ��
	SpanList()
	{
		_head = new Span;//��new _head���ǿ�
		_head->_next = _head;
		_head->_prev = _head;
	}

	void Insert(Span* pos, Span* newSpan)
	{
		//��posλ�ò���newSpan
		//����pos��newSpan
		assert(pos && newSpan);
		//��ʼ����
		Span* prev = pos->_prev;
		prev->_next = newSpan;
		newSpan->_next = pos;
		pos->_prev = newSpan;
		newSpan->_prev = prev;
	}

	void Erase(Span* pos)//���ﲢ��delete,delete�ͻ���ϵͳ��
	{
		//ɾ��posλ�õ�span
		//����span���ڱ�λ��ͷ���
		assert(pos&&pos!=_head);
		//��ʼɾ��
		Span* prev = pos->_prev;
		Span* next = pos->_next;
		prev->_next = next;
		next->_prev = prev;
	}

	//�ṩһ��begin��end�Ľӿڣ�ע��˫�����Ǵ�ͷ��
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
		//��֤��Ԫ����ͷɾ
		
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
	//Ͱ��
	std::mutex _mtx;
};
