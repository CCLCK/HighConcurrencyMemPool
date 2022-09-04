 #define _CRT_SECURE_NO_WARNINGS 1

#include "PageCache.h"

PageCache PageCache::_instance;
//获取一个K页的span给central cache
Span* PageCache::NewSpan(size_t k)
{
	//assert保证k大于0小于最大的页
	//assert(k > 0 && k < NPAGES);
	assert(k > 0 );

	//大于128页的的直接去找堆要
	//创建一个专属的Span描述这个大块内存，建立对应映射
	if (k>=NPAGES)
	{
		void* ptr = SystemAlloc(k);
		Span* span = _spanPool.New();
		span->_n = k;
		span->_pageId = ((PAGEID)ptr) >> PAGE_SHIFT;
		//建立映射
		_idSpanMap.set(span->_pageId, span);
		return span;
	}

	//1. 锁用递归锁，因为后面会递归调用自己
	//2. 借助子函数，主函数锁子函数
	//这里采用第一种，同时得把central cache的锁解了，可进一步提高效率 
	//锁的话可以锁整个函数，即在central cache调用NewSpan时加锁，这样就避免了死锁
	//同时也可以在central cache获取锁解掉
	 
	 
	//先去看第k个桶里有没有span
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
	//走到这说明没有，去找后面的桶，看后面的桶有没有
	//有的话就切分，没有的话准备找系统申请
	//切分：从头部切 得到K和n-k的span,k页的给central cache,n-k的挂到n-k对应的桶
	//切分span的本质是管理span的数据结构（起始页地址和span大小)
	for (int i=k+1;i<NPAGES;i++)
	{
		if (!_spanList[i].Empty())
		{
			Span* nSpan = _spanList[i].PopFront();
			//Span* kSpan =new Span;
			Span* kSpan = _spanPool.New();
			kSpan->_n = k;
			kSpan->_pageId = nSpan->_pageId;
			//开始切分
			nSpan->_n -= k;
			nSpan->_pageId +=k;//自己的页号往后推k页
			
			//切完后挂回去
			_spanList[nSpan->_n].PushFront(nSpan);

			//存储nSpan的首尾页，方便page cache回收
			//nSpan现在没有在用，映射首尾页方便回收即可
			//_idSpanMap[nSpan->_pageId] = nSpan;
			//_idSpanMap[nSpan->_pageId + nSpan->_n - 1] = nSpan;//注意是n-1
			_idSpanMap.set(nSpan->_pageId, nSpan);
			_idSpanMap.set(nSpan->_pageId + nSpan->_n - 1, nSpan);
			//对这个kSpan建立映射方便central cache回收小内存
			for (PAGEID i=0;i<kSpan->_n;i++)
			{
				//_idSpanMap[kSpan->_pageId + i] = kSpan;
				_idSpanMap.set(kSpan->_pageId + i, kSpan);

			}
			return kSpan;
		}
	}
	//到这说明pageCache找不到符合要求的了，得去找系统要
	//找系统要的大页span返回的是一个地址
	//把地址转成页号：除以8K
	//得到大页又需要切分：递归调用自己，减少代码复用  

	//开始申请
	//Span* bigSpan = new Span;
	Span* bigSpan = _spanPool.New();

	void* ptr=SystemAlloc(NPAGES - 1);
	bigSpan->_n = NPAGES - 1;
	bigSpan->_pageId = ((PAGEID)ptr) >> PAGE_SHIFT;
	
	//申请完挂起来
	_spanList[bigSpan->_n].PushFront(bigSpan);
	return NewSpan(k);
}


Span* PageCache::MapObjectToSpan(void* obj)//给一个对象就给其对应的span
{
	//根据地址算出页号（除以8k)
	PAGEID id = (PAGEID)obj >> PAGE_SHIFT;
	//去哈希表里去找这个页号，看能不能找到这个span
	//std::unique_lock<std::mutex>lock(_pageMtx);//RAII的锁,性能瓶颈
	//auto ret = _idSpanMap.find(id);
	//if (ret!=_idSpanMap.end())
	//{
	//	return ret->second;
	//}
	//else
	//{
	//	//找不到说明出问题了（说明指向的这个内存没有建立映射，assert
	//	assert(false);
	//}

	auto ret = (Span*)_idSpanMap.get(id);
	assert(ret != nullptr);
	return ret;
}


void PageCache::ReleaseSpanToPageCache(Span* span)
{
	//span太大了直接还给系统
	if (span->_n>=NPAGES)
	{
		void* ptr =(void*)(span->_pageId << PAGE_SHIFT);
		SystemFree(ptr);
		//delete span;//这个span是new出来的
		_spanPool.Delete(span);
		return;
	}
	//把central cache还回来的合并
	//对span前后的页尝试合并，缓解内存碎片问题
	//给一个标志_isUse标志这块有没有在用，防止central cache正在切分的被拿过来回收了
	//把正在使用的span的标志改成true
	//存储span前后页的映射，然后去找前后页，看能不能找到
	//根据span的_isUse和页号的映射以及合并后span的大小分情况判断


	//开始往前合并
	//改变span的属性
	while (1)
	{
		//找到前面的页
		PAGEID prevId = span->_pageId - 1;

		//没有前面的页
		//auto ret = _idSpanMap.find(prevId);
		auto ret = (Span*)_idSpanMap.get(prevId);
		//if (ret==_idSpanMap.end())
		if (ret==nullptr)
		{
			break;
		}
		//前面的页在使用了
		Span* prevSpan = ret;
		if (prevSpan->_isUse==true)
		{
			break;
		}
		//合并后的页大于等于NPAGES
		if (prevSpan->_n+span->_n>=NPAGES)
		{
			break;
		}

		//可以合并了
		span->_pageId = prevSpan->_pageId;
		span->_n += prevSpan->_n;
		_spanList[prevSpan->_n].Erase(prevSpan);//解决span的prev和next指针
		//把以前描述span的结构体delete了，两个合并成一个肯定有一个没了
		//delete prevSpan;
		_spanPool.Delete(prevSpan);

	}
	

	//再将其向后合并
	while (1)
	{
		//找到后面的页
		PAGEID nextId = span->_pageId+span->_n ;

		//没有后面的页
		//auto ret = _idSpanMap.find(nextId);
		auto ret = (Span*)_idSpanMap.get(nextId);
		if (ret == nullptr)
		{
			break;
		}
		//后面的页在使用了
		Span* nextSpan = ret;
		if (nextSpan->_isUse == true)
		{
			break;
		}
		//合并后的页大于等于NPAGES
		if (nextSpan->_n + span->_n >= NPAGES)
		{
			break;
		}

		//可以合并了
		span->_pageId = nextSpan->_pageId;
		span->_n += nextSpan->_n;
		_spanList[nextSpan->_n].Erase(nextSpan);//解决span的prev和next指针
		//把以前描述span的结构体delete了，两个合并成一个肯定有一个没了
		//delete nextSpan;
		_spanPool.Delete(nextSpan);

	}

	//把这个合并好的span挂回page cache
	_spanList[span->_n].PushFront(span);

	//合并后挂起来，同时建立首尾页的映射，将_isUse变为false
	/*_idSpanMap[span->_pageId] = span;
	_idSpanMap[span->_pageId+span->_n-1] = span;*/
	_idSpanMap.set(span->_pageId, span);
	_idSpanMap.set(span->_pageId + span->_n - 1, span);
	span->_isUse = false;

	//记得上锁

}


