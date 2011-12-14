#include "precomp.h"

CNodeProcessManager::CNodeProcessManager(CNodeApplication* application, IHttpContext* context)
	: application(application), processes(NULL), processCount(0), currentProcess(0), isClosing(FALSE),
	refCount(1)
{
	if (this->GetApplication()->IsDebugMode())
	{
		this->maxProcessCount = 1;
	}
	else
	{
		this->maxProcessCount = CModuleConfiguration::GetMaxProcessCountPerApplication(context);
	}

	// cache event provider since the application can be disposed prior to CNodeProcessManager
	this->eventProvider = this->GetApplication()->GetApplicationManager()->GetEventProvider();

	this->gracefulShutdownTimeout = CModuleConfiguration::GetGracefulShutdownTimeout(context);
	InitializeSRWLock(&this->srwlock);
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
}

CNodeApplication* CNodeProcessManager::GetApplication()
{
	return this->application;
}

HRESULT CNodeProcessManager::Initialize(IHttpContext* context)
{
	HRESULT hr;

	ErrorIf(NULL == (this->processes = new CNodeProcess* [this->maxProcessCount]), ERROR_NOT_ENOUGH_MEMORY);
	RtlZeroMemory(this->processes, this->maxProcessCount * sizeof(CNodeProcess*));
	if (this->GetApplication()->IsDebuggee())
	{
		// ensure the debugee process is started without activating message
		// this is to make sure it is available for the debugger to connect to

		CheckError(this->AddOneProcess(NULL, context));
	}

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
	CheckError(this->AddOneProcessCore(process, context));

	return S_OK;
Error:

	return hr;	
}

HRESULT CNodeProcessManager::Dispatch(CNodeHttpStoredContext* request)
{
	HRESULT hr;

	CheckNull(request);

	this->AddRef(); // decremented at the bottom of this method

	if (!this->isClosing) 
	{
		ENTER_SRW_SHARED(this->srwlock)

		if (!this->isClosing && this->TryRouteRequestToExistingProcess(request))
		{
			request = NULL;
		}

		LEAVE_SRW_SHARED(this->srwlock)

		if (request && !this->isClosing)
		{
			// existing processes were unable to accept this request; create a new process to handle it

			ENTER_SRW_EXCLUSIVE(this->srwlock)

			if (!this->isClosing && !this->TryRouteRequestToExistingProcess(request))
			{
				CNodeProcess* newProcess = NULL;
				CheckError(this->AddOneProcess(&newProcess, request->GetHttpContext()));
				CheckError(newProcess->AcceptRequest(request));
			}

			LEAVE_SRW_EXCLUSIVE(this->srwlock)
		}
	}

	this->DecRef(); // incremented at the beginning of this method

	return S_OK;
Error:

	this->GetEventProvider()->Log(
		L"iisnode failed to initiate processing of a request", WINEVENT_LEVEL_ERROR);

	if (!CProtocolBridge::SendIisnodeError(request, hr))
	{
		CProtocolBridge::SendEmptyResponse(request, 503, _T("Service Unavailable"), hr);
	}

	this->DecRef(); // incremented at the beginning of this method

	return hr;
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

HRESULT CNodeProcessManager::RecycleProcess(CNodeProcess* process)
{
	HRESULT hr;
	HANDLE recycler;
	ProcessRecycleArgs* args = NULL;
	BOOL gracefulRecycle = FALSE;
	CNodeApplication* appToRecycle = NULL;

	// remove the process from the process pool

	ENTER_SRW_EXCLUSIVE(this->srwlock)

	if (!this->isClosing)
	{
		appToRecycle = this->GetApplication();

		if (!appToRecycle->IsDebugMode())
		{
			appToRecycle = NULL;

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

				return S_OK;
			}

			if (i < (this->processCount - 1))
			{
				memcpy(this->processes + i, this->processes + i + 1, sizeof (CNodeProcess*) * (this->processCount - i - 1));
			}

			this->processCount--;
			this->currentProcess = 0;

			gracefulRecycle = TRUE;
		}
	}

	LEAVE_SRW_EXCLUSIVE(this->srwlock)

	// graceful recycle

	if (gracefulRecycle)
	{
		ErrorIf(NULL == (args = new ProcessRecycleArgs), ERROR_NOT_ENOUGH_MEMORY);
		args->count = 1;
		args->process = process;
		args->processes = &args->process;
		args->processManager = this;
		args->disposeApplication = FALSE;
		args->disposeProcess = TRUE;
		ErrorIf((HANDLE)-1L == (recycler = (HANDLE) _beginthreadex(NULL, 0, CNodeProcessManager::GracefulShutdown, args, 0, NULL)), ERROR_NOT_ENOUGH_MEMORY);
		CloseHandle(recycler);
	}

	if (appToRecycle)
	{
		appToRecycle->GetApplicationManager()->RecycleApplication(appToRecycle);
	}

	return S_OK;
Error:

	if (args)
	{
		delete args;
	}

	return hr;
}

HRESULT CNodeProcessManager::Recycle()
{
	HRESULT hr;
	HANDLE recycler;
	ProcessRecycleArgs* args = NULL;
	BOOL deleteApplication = FALSE;

	ENTER_SRW_EXCLUSIVE(this->srwlock)

	this->isClosing = TRUE;	

	if (0 < this->processCount)
	{
		// perform actual recycling on a diffrent thread to free up the file watcher thread

		ErrorIf(NULL == (args = new ProcessRecycleArgs), ERROR_NOT_ENOUGH_MEMORY);
		args->count = this->processCount;
		args->process = NULL;
		args->processes = this->processes;
		args->processManager = this;
		args->disposeApplication = TRUE;
		args->disposeProcess = FALSE;
		ErrorIf((HANDLE)-1L == (recycler = (HANDLE) _beginthreadex(NULL, 0, CNodeProcessManager::GracefulShutdown, args, 0, NULL)), ERROR_NOT_ENOUGH_MEMORY);
		CloseHandle(recycler);
	}
	else
	{
		deleteApplication = TRUE;
	}

	LEAVE_SRW_EXCLUSIVE(this->srwlock)

	if (deleteApplication)
	{
		delete this->GetApplication();
	}

	return S_OK;
Error:

	if (args)
	{
		delete args;
	}

	// if lack of memory is preventing us from initiating shutdown, we will just not provide auto update

	return hr;
}

unsigned int CNodeProcessManager::GracefulShutdown(void* arg)
{
	ProcessRecycleArgs* args = (ProcessRecycleArgs*)arg;
	HRESULT hr;
	HANDLE* drainHandles = NULL;

	// drain active requests 

	ErrorIf(NULL == (drainHandles = new HANDLE[args->count]), ERROR_NOT_ENOUGH_MEMORY);
	RtlZeroMemory(drainHandles, args->count * sizeof HANDLE);
	for (int i = 0; i < args->count; i++)
	{
		drainHandles[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
		args->processes[i]->SignalWhenDrained(drainHandles[i]);
	}
	
	if (args->processManager->gracefulShutdownTimeout > 0)
	{
		WaitForMultipleObjects(args->count, drainHandles, TRUE, args->processManager->gracefulShutdownTimeout);
	}

	for (int i = 0; i < args->count; i++)
	{
		CloseHandle(drainHandles[i]);
	}
	delete[] drainHandles;	
	drainHandles = NULL;

	if (args->disposeApplication)
	{
		// delete the application if requested (this will also delete process manager and all processes)
		// this is the application recycling code path

		delete args->processManager->GetApplication();
	}
	else if (args->disposeProcess)
	{
		// delete a single process if requested
		// this is the single process recycling code path (e.g. node.exe died)

		delete args->processes[0];
	}

	delete args;
	args = NULL;

	return S_OK;
Error:

	if (args)
	{
		delete args;
		args = NULL;
	}

	if (drainHandles)
	{
		delete [] drainHandles;
		drainHandles = NULL;
	}

	return hr;
}

long CNodeProcessManager::AddRef()
{
	return InterlockedIncrement(&this->refCount);
}

long CNodeProcessManager::DecRef()
{
	long result = InterlockedDecrement(&this->refCount);

	if (0 == result)
	{
		delete this;
	}

	return result;
}

CNodeEventProvider* CNodeProcessManager::GetEventProvider()
{
	return this->eventProvider;
}
