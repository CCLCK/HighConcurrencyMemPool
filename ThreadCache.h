#pragma once

#include "Common.h"

class ThreadCache
{
public:
	//������ͷŶ���ÿ���߳̽���TLSȥ����Allocate�����ڴ�
	//ÿ���̵߳���Deallocate�ͷ��ڴ�
	void* Allocate(size_t size);

	void Deallocate(void* ptr, size_t size);

	//�����Ļ����ȡ����(��thread cache����ʱ��
	//Fetch(��ȡ)������size��Ҫ�Ĵ�С
	//����index����size��Ӧ��Ͱ�ţ���������Ϊ�̻߳�������Ļ���ӳ�������ͬ
	void* FetchFromCentralCache(size_t index, size_t size);

	//�ͷŶ����������ʱ�������ڴ浽���Ļ���
	void ListTooLong(FreeList& list, size_t size);

private:
	FreeList _freeList[NFREELIST];
};

//ÿ���̶߳���һ���������˼���
static _declspec(thread) ThreadCache* pTLSThreadCache = nullptr;
//static��ֻ֤�ڵ�ǰ�ļ��ɼ�
  