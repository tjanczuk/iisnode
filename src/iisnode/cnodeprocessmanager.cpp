#include "precomp.h"

// TODO, tjanczuk, add detection of premature process termination
// TODO, tjanczuk, add support for job objects

CNodeProcessManager::CNodeProcessManager(CNodeApplication* application)
	: application(application), processes(NULL), processCount(0), currentProcess(0)
{
	this->maxProcessCount = CModuleConfiguration::GetMaxProcessCountPerApplication();
	InitializeCriticalSection(&this->syncRoot);
}

CNodeProcessManager::~CNodeProcessManager()
{
	if (NULL != processes)
	{
		for (int i = 0; i < this->processCount; i++)
		{
			delete this->processes[i];
		}

		delete[] this->processes;
		this->processes = NULL;
	}

	DeleteCriticalSection(&this->syncRoot);
}

CNodeApplication* CNodeProcessManager::GetApplication()
{
	return this->application;
}

HRESULT CNodeProcessManager::Initialize()
{
	HRESULT hr;

	ErrorIf(NULL == (this->processes = new CNodeProcess* [this->maxProcessCount]), ERROR_NOT_ENOUGH_MEMORY);
	RtlZeroMemory(this->processes, this->maxProcessCount * sizeof(CNodeProcess*));
	CheckError(this->AddOneProcess(NULL));

	return S_OK;
Error:

	if (NULL != this->processes)
	{
		delete [] this->processes;
		this->processes = NULL;
	}

	return hr;
}

HRESULT CNodeProcessManager::AddOneProcess(CNodeProcess** process)
{
	HRESULT hr;

	if (NULL != process)
	{
		*process = NULL;
	}

	ErrorIf(this->processCount == this->maxProcessCount, ERROR_NOT_ENOUGH_QUOTA);

	EnterCriticalSection(&this->syncRoot);

	ErrorIf(this->processCount == this->maxProcessCount, ERROR_NOT_ENOUGH_QUOTA);
	ErrorIf(NULL == (this->processes[this->processCount] = new CNodeProcess(this)), ERROR_NOT_ENOUGH_MEMORY);	
	CheckError(this->processes[this->processCount]->Initialize());
	if (NULL != process)
	{
		*process = this->processes[this->processCount];
	}
	this->processCount++;

	LeaveCriticalSection(&this->syncRoot);

	return S_OK;
Error:

	if (NULL != this->processes[this->processCount])
	{
		delete this->processes[this->processCount];
		this->processes[this->processCount] = NULL;
	}

	return hr;	
}

HRESULT CNodeProcessManager::TryDispatchOneRequest()
{
	HRESULT hr;

	CPendingRequestQueue* queue = this->GetApplication()->GetPendingRequestQueue();

	if (!queue->IsEmpty())
	{
		EnterCriticalSection(&this->syncRoot);

		CNodeHttpStoredContext* request = queue->Peek();
		BOOL requestDispatched = false;
		if (NULL != request)
		{
			if (!this->TryRouteRequestToExistingProcess(request))
			{
				CNodeProcess* newProcess;
				CheckError(this->AddOneProcess(&newProcess));
				requestDispatched = newProcess->TryAcceptRequest(request);
			}
			else 
			{
				requestDispatched = TRUE;
			}

			if (requestDispatched)
			{
				queue->Pop();
			}
		}

		LeaveCriticalSection(&this->syncRoot);
	}

	return S_OK;
Error:
	return hr;
}

BOOL CNodeProcessManager::TryRouteRequestToExistingProcess(CNodeHttpStoredContext* context)
{
	DWORD i = this->currentProcess;

	do {

		if (this->processes[i]->TryAcceptRequest(context))
		{
			return true;
		}

		i = (i + 1) % this->processCount;

	} while (i != this->currentProcess);

	return false;
}