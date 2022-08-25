#pragma once

#include <iostream>
#include <assert.h>
using std::cout;
using std::endl;

static const size_t NFREELIST = 208;//256KB�����ǵ�ӳ��һ����208��Ͱ
static const size_t MAXBYTES = 256 * 1024;


//�������뱣֤�ܹ��洢ҳ�Ų����
//64λ�»ᶨ��_WIN64��_WIN32�����꣬32λֻ�ᶨ��_WIN32




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
		return obj;//ͷɾ����ָ��
	}
	bool Empty()
	{
		return _freeList == nullptr;
	}
private:
	void* _freeList=nullptr;
	//maxSize
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
			assert(false);
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

private:
};


//����������ҳ�Ĵ���ڴ��Ƚṹ
//���Լǵó�ʼ��
struct Span
{
	//����ڴ���ʼҳ��ҳ��
	//64λ����ÿҳ8KBΪ��һ����2^64/2^13=2^51ҳ��int�治�£���unsigned long long+����������

	//ҳ������
	
	//˫������Ľṹ


	//�кõ�С���ڴ汻�����thread cache�ļ���

	//�кõ�С���ڴ����������
	
	//��ǰspan�Ƿ��ڱ�ʹ��


};

//��ͷ˫��ѭ��������Span������
class SpanList
{

public:
	//˫��ѭ�������ʼ��
	SpanList()
	{

	}

	void Insert(Span* pos, Span* newSpan)
	{
		//��posλ�ò���newSpan

		//����pos��newSpan

		//��ʼ����
	}

	void Erase(Span* pos)
	{
		//ɾ��posλ�õ�span

		//����span���ڱ�λ��ͷ���

		//��ʼɾ��
	}


private:
	Span* _head;

public:
	//Ͱ��
};
