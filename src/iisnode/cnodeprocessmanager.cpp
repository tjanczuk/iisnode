#include "precomp.h"

CNodeProcessManager::CNodeProcessManager(CNodeApplication* application, IHttpContext* context)
	: application(application), processes(NULL), processCount(0), currentProcess(0)
{
	this->maxProcessCount = CModuleConfiguration::GetMaxProcessCountPerApplication(context);
	this->gracefulShutdownTimeout = CModuleConfiguration::GetGracefulShutdownTimeout(context);
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

	return S_OK;
Error:

	if (NULL != this->processes)
	{
		delete [] this->processes;
		this->processes = NULL;
	}

	return hr;
}

HRESULT CNodeProcessManager::AddOneProcessCore(CNodeProcess** process, IHttpContext* context)
{
	HRESULT hr;

	ErrorIf(this->processCount == this->maxProcessCount, ERROR_NOT_ENOUGH_QUOTA);
	ErrorIf(NULL == (this->processes[this->processCount] = new CNodeProcess(this, context, this->processCount)), ERROR_NOT_ENOUGH_MEMORY);	
	CheckError(this->processes[this->processCount]->Initialize(context));
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

HRESULT CNodeProcessManager::AddOneProcess(CNodeProcess** process, IHttpContext* context)
{
	HRESULT hr;

	if (NULL != process)
	{
		*process = NULL;
	}

	ErrorIf(this->processCount == this->maxProcessCount, ERROR_NOT_ENOUGH_QUOTA);

	ENTER_CS(this->syncRoot)

	CheckError(this->AddOneProcessCore(process, context));

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
				CheckError(this->AddOneProcess(&newProcess, request->GetHttpContext()));
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
	if (this->processCount == 0)
	{
		return false;
	}

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

void CNodeProcessManager::RecycleProcess(CNodeProcess* process)
{
	// remove the process from the process pool

	ENTER_CS(this->syncRoot)

	int i;
	for (i = 0; i < this->processCount; i++)
	{
		if (this->processes[i] == process)
		{
			break;
		}
	}

	if (i == this->processCount)
	{
		// process not found in the active process list

		return;
	}

	if (i < (this->processCount - 1))
	{
		memcpy(this->processes + i, this->processes + i + 1, sizeof (CNodeProcess*) * (this->processCount - i - 1));
	}

	this->processCount--;
	this->currentProcess = 0;

	LEAVE_CS(this->syncRoot)
}

void CNodeProcessManager::RecycleAllProcesses()
{
	HRESULT hr;
	ProcessRecycleArgs* args;
	HANDLE recycler;

	ErrorIf(NULL == (args = new ProcessRecycleArgs), ERROR_NOT_ENOUGH_MEMORY);
	RtlZeroMemory(args, sizeof ProcessRecycleArgs);
	args->gracefulShutdownTimeout = this->gracefulShutdownTimeout;

	ENTER_CS(this->syncRoot)

	ErrorIf(0 == this->processCount, S_OK); // bail out if there is no process to recycle

	args->count = this->processCount;
	ErrorIf(NULL == (args->processes = new CNodeProcess* [args->count]), ERROR_NOT_ENOUGH_MEMORY);
	memcpy(args->processes, this->processes, args->count * sizeof (CNodeProcess*));
	this->processCount = 0;
	this->currentProcess = 0;

	LEAVE_CS(this->syncRoot)

	// perform actual recycling on a diffrent thread to free up the file watcher thread

	ErrorIf((HANDLE)-1L == (recycler = (HANDLE) _beginthreadex(NULL, 0, CNodeProcessManager::GracefulShutdown, args, 0, NULL)), ERROR_NOT_ENOUGH_MEMORY);
	CloseHandle(recycler);

	return;
Error:

	if (NULL != args)
	{
		if (NULL != args->processes)
		{			
			// fallback to ungraceful shutdown
			for (int i = 0; i < args->count; i++)
			{
				delete args->processes[i];
			}

			delete [] args->processes;
		}

		delete args;
	}

	// if lack of memory is preventing us from initiating shutdown, we will just not provide auto update

	return;
}

unsigned int CNodeProcessManager::GracefulShutdown(void* arg)
{
	ProcessRecycleArgs* args = (ProcessRecycleArgs*)arg;
	HRESULT hr;
	HANDLE* drainHandles;
	DWORD drainHandleCount = 0;

	ErrorIf(NULL == (drainHandles = new HANDLE[args->count]), ERROR_NOT_ENOUGH_MEMORY);
	RtlZeroMemory(drainHandles, args->count * sizeof HANDLE);
	for (int i = 0; i < args->count; i++)
	{
		drainHandles[drainHandleCount] = args->processes[i]->CreateDrainHandle();
		if (INVALID_HANDLE_VALUE != drainHandles[drainHandleCount])
		{
			drainHandleCount++;
		}
	}
	
	if (args->gracefulShutdownTimeout > 0)
	{
		WaitForMultipleObjects(drainHandleCount, drainHandles, TRUE, args->gracefulShutdownTimeout);
	}

	for (int i = 0; i < drainHandleCount; i++)
	{
		CloseHandle(drainHandles[i]);
	}
	delete[] drainHandles;

	for (int i = 0; i < args->count; i++)
	{
		delete args->processes[i];
	}
	delete args->processes;

	delete args;

	return S_OK;
Error:
	return hr;
}