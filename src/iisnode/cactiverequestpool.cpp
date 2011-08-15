#include "precomp.h"

CActiveRequestPool::CActiveRequestPool()
{
	InitializeCriticalSection(&this->syncRoot);
}

CActiveRequestPool::~CActiveRequestPool()
{
	DeleteCriticalSection(&this->syncRoot);
}

HRESULT CActiveRequestPool::Add(CNodeHttpStoredContext* context)
{
	HRESULT hr;

	CheckNull(context);

	ENTER_CS(this->syncRoot)

	ErrorIf(this->requests.size() >= CModuleConfiguration::GetMaxConcurrentRequestsPerProcess(), ERROR_NOT_ENOUGH_QUOTA);
	this->requests.push_back(context);

	LEAVE_CS(this->syncRoot)

	return S_OK;
Error:
	return hr;
}

HRESULT CActiveRequestPool::Remove(CNodeHttpStoredContext* context)
{
	HRESULT hr;

	CheckNull(context);

	ENTER_CS(this->syncRoot)

	this->requests.remove(context);

	LEAVE_CS(this->syncRoot)

	return S_OK;
Error:
	return hr;
}