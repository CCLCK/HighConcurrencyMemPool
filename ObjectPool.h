#pragma once

#include <iostream>
#include "Common.h"
using std::cout;
using std::endl;

//#ifdef _WIN32
//#include<windows.h>
//#else 
////linux ...
//#endif

//// ֱ��ȥ���ϰ�ҳ����ռ�
//inline static void* SystemAlloc(size_t kpage)
//{
//#ifdef _WIN32
//	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
//#else
//	// linux��brk mmap��
//#endif
//
//	if (ptr == nullptr)
//		throw std::bad_alloc();
//
//	return ptr;
//}
//
//void*& NextObj(void* obj)
//{
//	return *((void**)obj);//�õ�����ڴ���ͷ4/8���ֽڣ���ͬ��nextָ�룬�����洢��һ�����ĵ�ַ
//}

//�����ڴ�أ�����ȥ�У��ض������Ч�ʱ�malloc�Ըߣ�
template<class T>//ÿ�δ���һ��T����
class ObjectPool
{
public:
	T* New()//����һ��T����
	{
		T* obj;
		
		
		//����������������������������һ��������ͷɾ
		//ͷ�ϵľ���_freeListָ���
		if (_freeList != nullptr)
		{
			obj = (T*)_freeList;
			_freeList = NextObj(_freeList);
		}
		else
		{
			//������û�У����ӣ�����ڴ棩��ʣ�µ��ֽڲ���һ������ʱ���¿���
			//���¿��ٵ�ʱ�����ϵͳapi������malloc
			if (_remainBytes<sizeof(T))
			{
				_remainBytes = 16<<13;//
				_memory =(char*)SystemAlloc(_remainBytes>>13);//2^13==8KB,�ǵ�ǿת����ֵ������	
				if (_memory==nullptr)//����ʧ�����쳣
				{
					throw std::bad_alloc();
				}
			}
			obj = (T*)_memory;
			//��������T��char�治��һ��ָ��͸�һ��ָ��
			//T���ʹ������T�����Ƕ��͸����
			size_t actualNum = sizeof(T) > sizeof(void*) ? sizeof(T) : sizeof(void*);
			_memory += actualNum;//��Ҳ��ΪɶҪ��char*
			_remainBytes -= actualNum;
		}
		
		//����Ŀ����˿ռ䣬�����Ҫ��ʼ����
		//malloc��ʼ�����ö�λnew
		new(obj)T;
		return obj;
	}
	void Delete(T* obj)
	{
		//��ʽ�ĵ�����������
		obj->~T();
		//���������������������ͷ��
		NextObj(obj) = _freeList;
		_freeList = obj;
	}

private:
	char* _memory=nullptr;//���ٵ�ָ�����ڴ��ָ��
	void* _freeList=nullptr;//���������ͷָ��
	//ʹ�ö����ڴ�ʱһֱ����,��������Ҫ8�ֽ�ֻ��5�ֽ��˾���Ҫ���¿��ٲ�������5�ֽ�
	size_t _remainBytes = 0;
	
};

#include<vector>
struct TreeNode
{
	int _val;
	TreeNode* _left;
	TreeNode* _right;

	TreeNode()
		:_val(0)
		, _left(nullptr)
		, _right(nullptr)
	{}
};

void TestObjectPool()
{
	// �����ͷŵ��ִ�
	const size_t Rounds = 5;

	// ÿ�������ͷŶ��ٴ�
	const size_t N = 1000000;

	std::vector<TreeNode*> v1;
	v1.reserve(N);

	size_t begin1 = clock();
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v1.push_back(new TreeNode);
		}
		for (int i = 0; i < N; ++i)
		{
			delete v1[i];
		}
		v1.clear();
	}

	size_t end1 = clock();

	std::vector<TreeNode*> v2;
	v2.reserve(N);

	ObjectPool<TreeNode> TNPool;
	size_t begin2 = clock();
	for (size_t j = 0; j < Rounds; ++j)
	{
		for (int i = 0; i < N; ++i)
		{
			v2.push_back(TNPool.New());
		}
		for (int i = 0; i < N; ++i)
		{
			TNPool.Delete(v2[i]);
		}
		v2.clear();
	}
	size_t end2 = clock();

	cout << "new cost time:" << end1 - begin1 << endl;
	cout << "object pool cost time:" << end2 - begin2 << endl;
}













