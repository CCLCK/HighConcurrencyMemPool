#define _CRT_SECURE_NO_WARNINGS 1


#include "ObjectPool.h"
#include "ConcurrentAlloc.h"
int main()
{
	//TestObjectPool();
	ConcurrentAlloc(50);
	ConcurrentAlloc(50);
	ConcurrentAlloc(50);
	return 0;
}
