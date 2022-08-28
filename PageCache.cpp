 #define _CRT_SECURE_NO_WARNINGS 1


#include "PageCache.h"

PageCache PageCache::_instance;
//获取一个K页的span给central cache
Span* PageCache::NewSpan(size_t k)
{
	//assert保证k大于0小于最大的页
	assert(k > 0 && k < NPAGES);
	//1. 锁用递归锁，因为后面会递归调用自己
	//2. 借助子函数，主函数锁子函数
	//这里采用第一种，同时得把central cache的锁解了，可进一步提高效率 
	//锁的话可以锁整个函数，即在central cache调用NewSpan时加锁，这样就避免了死锁
	//同时也可以在central cache获取锁解掉
	 
	 
	//先去看第k个桶里有没有span
	if (!_spanList[k].Empty())
	{
		Span* kSpan = _spanList->PopFront();
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
			Span* kSpan =new Span;
			kSpan->_n = k;
			kSpan->_pageId = nSpan->_pageId;
			//开始切分
			nSpan->_n -= k;
			nSpan->_pageId +=k;//自己的页号往后推k页
			
			//切完后挂回去
			_spanList[nSpan->_n].PushFront(nSpan);
			return kSpan;
		}
	}
	//到这说明pageCache找不到符合要求的了，得去找系统要
	//找系统要的大页span返回的是一个地址
	//把地址转成页号：除以8K
	//得到大页又需要切分：递归调用自己，减少代码复用  

	//开始申请
	Span* bigSpan = new Span;
	void* ptr=SystemAlloc(NPAGES - 1);
	bigSpan->_n = NPAGES - 1;
	bigSpan->_pageId = ((PAGEID)ptr) >> PAGE_SHIFT;
	
	//申请完挂起来
	_spanList[bigSpan->_n].PushFront(bigSpan);
	return NewSpan(k);
}






