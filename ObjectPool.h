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

//// 直接去堆上按页申请空间
//inline static void* SystemAlloc(size_t kpage)
//{
//#ifdef _WIN32
//	void* ptr = VirtualAlloc(0, kpage << 13, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
//#else
//	// linux下brk mmap等
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
//	return *((void**)obj);//得到这个内存块的头4/8个字节，等同于next指针，用来存储下一个结点的地址
//}

//对象内存池（定长去切，特定情况下效率比malloc略高）
template<class T>//每次创建一个T对象
class ObjectPool
{
public:
	T* New()//申请一个T对象
	{
		T* obj;
		
		
		//优先找自由链表，从自由链表里拿一个出来，头删
		//头上的就是_freeList指向的
		if (_freeList != nullptr)
		{
			obj = (T*)_freeList;
			_freeList = NextObj(_freeList);
		}
		else
		{
			//链表里没有，池子（大块内存）里剩下的字节不够一个对象时重新开辟
			//重新开辟的时候调用系统api，跳过malloc
			if (_remainBytes<sizeof(T))
			{
				_remainBytes = 16<<13;//
				_memory =(char*)SystemAlloc(_remainBytes>>13);//2^13==8KB,记得强转返回值的类型	
				if (_memory==nullptr)//分配失败抛异常
				{
					throw std::bad_alloc();
				}
			}
			obj = (T*)_memory;
			//如果申请的T是char存不下一个指针就给一个指针
			//T类型存的下那T类型是多大就给多大
			size_t actualNum = sizeof(T) > sizeof(void*) ? sizeof(T) : sizeof(void*);
			_memory += actualNum;//这也是为啥要用char*
			_remainBytes -= actualNum;
		}
		
		//上面的开好了空间，下面就要初始化了
		//malloc初始化调用定位new
		new(obj)T;
		return obj;
	}
	void Delete(T* obj)
	{
		//显式的调用析构函数
		obj->~T();
		//链入自由链表，单链表采用头插
		NextObj(obj) = _freeList;
		_freeList = obj;
	}

private:
	char* _memory=nullptr;//开辟的指向大块内存的指针
	void* _freeList=nullptr;//自由链表的头指针
	//使用定长内存时一直在切,到最后可能要8字节只有5字节了就需要重新开辟并舍弃这5字节
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
	// 申请释放的轮次
	const size_t Rounds = 5;

	// 每轮申请释放多少次
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













