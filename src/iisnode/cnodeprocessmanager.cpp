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

HRESULT CNodeProcessManager::AddOneProcessCore(CNodeProcess** process)
{
	HRESULT hr;

	ErrorIf(this->processCount == this->maxProcessCount, ERROR_NOT_ENOUGH_QUOTA);
	ErrorIf(NULL == (this->processes[this->processCount] = new CNodeProcess(this)), ERROR_NOT_ENOUGH_MEMORY);	
	CheckError(this->processes[this->processCount]->Initialize());
	if (NULL != process)
	{
		*process = this->processes[this->processCount];
	}
	this->processCount++;

	return S_OK;
Error:

	if (NULL != this->processes[this->processCount])
	{
		delete this->processes[this->processCount];
		this->processes[this->processCount] = NULL;
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

	ENTER_CS(this->syncRoot)

	CheckError(this->AddOneProcessCore(process));

	LEAVE_CS(this->syncRoot)

	return S_OK;
Error:

	return hr;	
}

void CNodeProcessManager::TryDispatchOneRequest(void* data)
{
	((CNodeProcessManager*)data)->TryDispatchOneRequestImpl();
}

void CNodeProcessManager::TryDispatchOneRequestImpl()
{
	HRESULT hr;
	CNodeHttpStoredContext* request = NULL;
	CPendingRequestQueue* queue = this->GetApplication()->GetPendingRequestQueue();

	if (!queue->IsEmpty())
	{
		ENTER_CS(this->syncRoot)

		request = queue->Peek();		
		if (NULL != request)
		{
			queue->Pop();

			if (!this->TryRouteRequestToExistingProcess(request))
			{
				CNodeProcess* newProcess;
				CheckError(this->AddOneProcess(&newProcess));
				CheckError(newProcess->AcceptRequest(request));
			}
		}

		LEAVE_CS(this->syncRoot)
	}

	return;
Error:

	if (request != NULL)
	{
		CProtocolBridge::SendEmptyResponse(request, 503, _T("Service Unavailable"), hr);
	}

	return;
}

BOOL CNodeProcessManager::TryRouteRequestToExistingProcess(CNodeHttpStoredContext* context)
{
	DWORD i = this->currentProcess;

	do {

		if (S_OK == this->processes[i]->AcceptRequest(context))
		{
			return true;
		}

		i = (i + 1) % this->processCount;

	} while (i != this->currentProcess);

	return false;
}