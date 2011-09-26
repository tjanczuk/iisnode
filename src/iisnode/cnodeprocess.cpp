#include "precomp.h"

CNodeProcess::CNodeProcess(CNodeProcessManager* processManager, IHttpContext* context, DWORD ordinal)
	: processManager(processManager), process(NULL), processWatcher(NULL), isClosing(0), ordinal(ordinal)
{
	RtlZeroMemory(this->namedPipe, sizeof this->namedPipe);
	RtlZeroMemory(&this->startupInfo, sizeof this->startupInfo);
	this->maxConcurrentRequestsPerProcess = CModuleConfiguration::GetMaxConcurrentRequestsPerProcess(context);
}

CNodeProcess::~CNodeProcess()
{
	this->isClosing = 1;

	if (NULL != this->process)
	{
		TerminateProcess(this->process, 2);
		CloseHandle(this->process);
		this->process = NULL;
	}

	if (NULL != this->processWatcher)
	{
		WaitForSingleObject(this->processWatcher, INFINITE);
		CloseHandle(this->processWatcher);
		this->processWatcher = NULL;
	}

	if (NULL != this->startupInfo.hStdOutput && INVALID_HANDLE_VALUE != this->startupInfo.hStdOutput)
	{
		CloseHandle(this->startupInfo.hStdOutput);		
		this->startupInfo.hStdOutput = INVALID_HANDLE_VALUE;
	}

	if (NULL != this->startupInfo.hStdError && INVALID_HANDLE_VALUE != this->startupInfo.hStdError)
	{
		CloseHandle(this->startupInfo.hStdError);
		this->startupInfo.hStdError = INVALID_HANDLE_VALUE;
	}	
}

HRESULT CNodeProcess::Initialize(IHttpContext* context)
{
	HRESULT hr;
	UUID uuid;
	RPC_CSTR suuid = NULL;
	LPTSTR fullCommandLine = NULL;
	LPCTSTR coreCommandLine;
	PCWSTR scriptName;
	size_t coreCommandLineLength, scriptNameLength, scriptNameLengthW;	
	PROCESS_INFORMATION processInformation;
	DWORD exitCode = S_OK;
	LPCH newEnvironment = NULL;
	DWORD flags;
	HANDLE job;
	PWSTR currentDirectory = NULL;
	PSTR currentDirectoryA = NULL;
	DWORD currentDirectorySize = 0;
	DWORD currentDirectorySizeA = 0;

	RtlZeroMemory(&processInformation, sizeof processInformation);
	RtlZeroMemory(&startupInfo, sizeof startupInfo);

	// configure logging

	if (TRUE == (this->loggingEnabled = CModuleConfiguration::GetLoggingEnabled(context)))
	{
		this->logFlushInterval = CModuleConfiguration::GetLogFileFlushInterval(context);
		this->maxLogSizeInBytes = (LONGLONG)CModuleConfiguration::GetMaxLogFileSizeInKB(context) * (LONGLONG)1024;
	}

	// generate the name for the named pipe to communicate with the node.js process
	
	ErrorIf(RPC_S_OK != UuidCreate(&uuid), ERROR_CAN_NOT_COMPLETE);
	ErrorIf(RPC_S_OK != UuidToString(&uuid, &suuid), ERROR_NOT_ENOUGH_MEMORY);
	_tcscpy(this->namedPipe, _T("\\\\.\\pipe\\"));
	_tcscpy(this->namedPipe + 9, (char*)suuid);
	RpcStringFree(&suuid);
	suuid = NULL;

	// build the full command line for the node.js process

	coreCommandLine = CModuleConfiguration::GetNodeProcessCommandLine(context);
	scriptName = this->GetProcessManager()->GetApplication()->GetScriptName();
	coreCommandLineLength = _tcslen(coreCommandLine);
	scriptNameLengthW = wcslen(scriptName) + 1;
	ErrorIf(0 != wcstombs_s(&scriptNameLength, NULL, 0, scriptName, _TRUNCATE), ERROR_CAN_NOT_COMPLETE);
	ErrorIf(NULL == (fullCommandLine = new TCHAR[coreCommandLineLength + scriptNameLength + 2 + 1]), ERROR_NOT_ENOUGH_MEMORY); // script name must be enclosed in quotes
	_tcscpy(fullCommandLine, coreCommandLine);
	_tcscat(fullCommandLine, _T(" \""));
	ErrorIf(0 != wcstombs_s(&scriptNameLength, fullCommandLine + coreCommandLineLength + 1 + 1, scriptNameLength, scriptName, _TRUNCATE), ERROR_CAN_NOT_COMPLETE);
	_tcscat(fullCommandLine, _T("\""));

	// create the environment block for the node.js process 	

	CheckError(CModuleConfiguration::CreateNodeEnvironment(context, this->namedPipe, &newEnvironment));

	// establish the current directory for node.exe process to be the same as the location of the *.js file

	currentDirectory = (PWSTR)context->GetPhysicalPath(&currentDirectorySize);
	while (currentDirectorySize && currentDirectory[currentDirectorySize] != L'\\' && currentDirectory[currentDirectorySize] != L'/')
		currentDirectorySize--;
	ErrorIf(0 == (currentDirectorySizeA = WideCharToMultiByte(CP_ACP, 0, currentDirectory, currentDirectorySize, NULL, 0, NULL, NULL)), E_FAIL);
	ErrorIf(NULL == (currentDirectoryA = new char[currentDirectorySize + 1]), ERROR_NOT_ENOUGH_MEMORY);
	ErrorIf(currentDirectorySizeA != WideCharToMultiByte(CP_ACP, 0, currentDirectory, currentDirectorySize, currentDirectoryA, currentDirectorySizeA, NULL, NULL), E_FAIL);
	currentDirectoryA[currentDirectorySizeA] = '\0';

	// create startup info for the node.js process

	RtlZeroMemory(&this->startupInfo, sizeof this->startupInfo);
	GetStartupInfo(&startupInfo);
	CheckError(this->CreateStdHandles(context));

	// create process watcher thread in a suspended state

	ErrorIf(NULL == (this->processWatcher = (HANDLE)_beginthreadex(
		NULL, 
		4096, 
		CNodeProcess::ProcessWatcher, 
		this, 
		CREATE_SUSPENDED, 
		NULL)), 
		ERROR_NOT_ENOUGH_MEMORY);

	// create the node.exe process

	flags = DETACHED_PROCESS | CREATE_SUSPENDED;
	if (this->GetProcessManager()->GetApplication()->GetApplicationManager()->GetBreakAwayFromJobObject())
	{
		flags |= CREATE_BREAKAWAY_FROM_JOB;
	}
	
	ErrorIf(FALSE == CreateProcess(
			NULL,
			fullCommandLine,
			NULL,
			NULL,
			TRUE,
			flags,
			newEnvironment,
			currentDirectoryA,
			&this->startupInfo,
			&processInformation
		), GetLastError());

	// duplicate stdout and stderr handles to allow flushing without regard for whether the node process exited
	// closing of the original handles will be taken care of by the newly started process

	if (INVALID_HANDLE_VALUE != this->startupInfo.hStdOutput && NULL != this->startupInfo.hStdOutput)
	{
		ErrorIf(0 == DuplicateHandle(
			GetCurrentProcess(), 
			this->startupInfo.hStdOutput, 
			GetCurrentProcess(), 
			&this->startupInfo.hStdOutput, 
			0, 
			TRUE, 
			DUPLICATE_SAME_ACCESS),
			GetLastError());
	}

	if (INVALID_HANDLE_VALUE != this->startupInfo.hStdError && NULL != this->startupInfo.hStdError)
	{
		ErrorIf(0 == DuplicateHandle(
			GetCurrentProcess(), 
			this->startupInfo.hStdError, 
			GetCurrentProcess(), 
			&this->startupInfo.hStdError, 
			0, 
			TRUE, 
			DUPLICATE_SAME_ACCESS),
			GetLastError());
	}

	// join a job object if needed, then resume the process

	job = this->GetProcessManager()->GetApplication()->GetApplicationManager()->GetJobObject();
	if (NULL != job)
	{
		ErrorIf(!AssignProcessToJobObject(job, processInformation.hProcess), HRESULT_FROM_WIN32(GetLastError()));
	}

	ErrorIf((DWORD) -1 == ResumeThread(processInformation.hThread), GetLastError());
	ErrorIf(GetExitCodeProcess(processInformation.hProcess, &exitCode) && STILL_ACTIVE != exitCode, exitCode);
	this->process = processInformation.hProcess;

	// start process watcher thread to get notified of premature node process termination in CNodeProcess::OnProcessExited		

	ResumeThread(this->processWatcher);

	// clean up

	delete [] currentDirectoryA;
	currentDirectoryA = NULL;
	delete [] newEnvironment;
	newEnvironment = NULL;
	delete [] fullCommandLine;
	fullCommandLine = NULL;
	CloseHandle(processInformation.hThread);
	processInformation.hThread = NULL;	

	return S_OK;
Error:

	if (currentDirectoryA)
	{
		delete [] currentDirectoryA;
		currentDirectoryA = NULL;
	}

	if (suuid != NULL)
	{
		RpcStringFree(&suuid);
		suuid = NULL;
	}

	if (NULL != fullCommandLine)
	{
		delete[] fullCommandLine;
		fullCommandLine = NULL;
	}

	if (NULL != processInformation.hThread)
	{
		CloseHandle(processInformation.hThread);
		processInformation.hThread = NULL;
	}

	if (NULL != processInformation.hProcess)
	{
		TerminateProcess(processInformation.hProcess, 1);
		CloseHandle(processInformation.hProcess);
		processInformation.hProcess = NULL;
	}

	if (NULL != newEnvironment)
	{
		delete[] newEnvironment;
		newEnvironment = NULL;
	}

	if (NULL != this->processWatcher)
	{
		ResumeThread(this->processWatcher);
		WaitForSingleObject(this->processWatcher, INFINITE);
		CloseHandle(this->processWatcher);
		this->processWatcher = NULL;
	}

	if (NULL != this->startupInfo.hStdOutput && INVALID_HANDLE_VALUE != this->startupInfo.hStdOutput)
	{
		CloseHandle(this->startupInfo.hStdOutput);		
		this->startupInfo.hStdOutput = INVALID_HANDLE_VALUE;
	}

	if (NULL != this->startupInfo.hStdError && INVALID_HANDLE_VALUE != this->startupInfo.hStdError)
	{
		CloseHandle(this->startupInfo.hStdError);
		this->startupInfo.hStdError = INVALID_HANDLE_VALUE;
	}	

	return hr;
}

void CNodeProcess::FlushStdHandles()
{
	LARGE_INTEGER fileSize;

	// truncate the log file back to 0 if the max size is exceeded

	if (GetFileSizeEx(this->startupInfo.hStdOutput, &fileSize))
	{
		if (fileSize.QuadPart > this->maxLogSizeInBytes)
		{
			fileSize.QuadPart = 0;
			if (SetFilePointerEx(this->startupInfo.hStdOutput, fileSize, NULL, FILE_BEGIN))
			{
				SetEndOfFile(this->startupInfo.hStdOutput);
			}
		}
	}

	// flush the file

	FlushFileBuffers(this->startupInfo.hStdOutput);
}

unsigned int WINAPI CNodeProcess::ProcessWatcher(void* arg)
{
	CNodeProcess* process = (CNodeProcess*)arg;
	DWORD exitCode;

	while (process->process)
	{
		if (WAIT_TIMEOUT == WaitForSingleObject(process->process, process->loggingEnabled ? process->logFlushInterval : INFINITE))
		{
			process->FlushStdHandles();
		}
		else if (GetExitCodeProcess(process->process, &exitCode) && STILL_ACTIVE != exitCode)
		{
			process->OnProcessExited();
			return exitCode;
		}
	}

	return 0;
}

CNodeProcessManager* CNodeProcess::GetProcessManager()
{
	return this->processManager;
}

HRESULT CNodeProcess::AcceptRequest(CNodeHttpStoredContext* context)
{
	HRESULT hr;

	CheckError(this->activeRequestPool.Add(context));
	context->SetNodeProcess(this);
	CProtocolBridge::InitiateRequest(context);

	return S_OK;
Error:

	return hr;
}

LPCTSTR CNodeProcess::GetNamedPipeName()
{
	return this->namedPipe;
}

void CNodeProcess::OnRequestCompleted(CNodeHttpStoredContext* context)
{
	// capture asyncManager and processManager before call to Remove, since Remove may cause a race condition 
	// with 'delete this' in CNodeProcess::OnProcessExited which executes on a different thread

	CAsyncManager* asyncManager = this->GetProcessManager()->GetApplication()->GetApplicationManager()->GetAsyncManager();
	CNodeProcessManager* processManager = this->GetProcessManager();
	
	this->activeRequestPool.Remove(context);
	
	asyncManager->PostContinuation(CNodeProcessManager::TryDispatchOneRequest, processManager);
}

void CNodeProcess::OnProcessExited()
{
	CloseHandle(this->process);
	this->process = NULL;
	CloseHandle(this->processWatcher);
	this->processWatcher = NULL;

	// OnProcessExited should be mutually exclusive with CreateDrainHandle
	// Both will eventually recycle the process, and only one of them may execute in the 
	// lifetime of CNodeProcess.

	if (0 == InterlockedExchange(&this->isClosing, 1))
	{
		this->GetProcessManager()->RecycleProcess(this);

		// at this point no new requests will be dispatched to this process;
		// wait for active requests to gracefully drain, then destruct itself
		// this should be reasonably prompt since all named pipes are broken
		
		HANDLE drainHandle = CreateEvent(NULL, TRUE, FALSE, NULL);
		if (NULL != drainHandle)
		{
			this->activeRequestPool.SignalWhenDrained(drainHandle);
			WaitForSingleObject(drainHandle, INFINITE);
			CloseHandle(drainHandle);
		}

		delete this;
	}
}

HANDLE CNodeProcess::CreateDrainHandle()
{
	// OnProcessExited should be mutually exclusive with CreateDrainHandle
	// Both will eventually recycle the process, and only one of them may execute in the 
	// lifetime of CNodeProcess.

	if (0 == InterlockedExchange(&this->isClosing, 1))
	{
		HANDLE drainHandle = CreateEvent(NULL, TRUE, FALSE, NULL);
		this->activeRequestPool.SignalWhenDrained(drainHandle);

		return drainHandle;
	}
	else
	{
		return NULL;
	}
}

HRESULT CNodeProcess::CreateStdHandles(IHttpContext* context)
{
	this->startupInfo.hStdError = this->startupInfo.hStdInput = this->startupInfo.hStdOutput = INVALID_HANDLE_VALUE;
	if (!this->loggingEnabled)
	{
		this->startupInfo.dwFlags = 0;
		return S_OK;
	}
	else
	{
		this->startupInfo.dwFlags = STARTF_USESTDHANDLES;
	}

	HRESULT hr;
	PCWSTR scriptName;
	LPWSTR logComponentName = CModuleConfiguration::GetLogDirectoryNameSuffix(context);
	WCHAR* logName = NULL;
	SECURITY_ATTRIBUTES security;
	DWORD creationDisposition;

	CheckNull(logComponentName);	

	// allocate enough memory to store log file name of the form <scriptName>.<logComponentName>\<ordinalProcessNumber>.txt

	scriptName = this->GetProcessManager()->GetApplication()->GetScriptName();
	ErrorIf(NULL == (logName = new WCHAR[wcslen(scriptName) + 1 + wcslen(logComponentName) + 10 + 4 + 1]), ERROR_NOT_ENOUGH_MEMORY); 

	// ensure a directory for storing the log file exists; the directory name is of the form <scriptName>.<logComponentName>

	swprintf(logName, L"%s.%s", scriptName, logComponentName);
	if (!CreateDirectoryW(logName, NULL))
	{
		hr = GetLastError();
		ErrorIf(ERROR_ALREADY_EXISTS != hr, hr);
	}

	// create log file name

	swprintf(logName, L"%s.%s\\%d.txt", scriptName, logComponentName, this->ordinal);

	// stdout == stderr

	RtlZeroMemory(&security, sizeof SECURITY_ATTRIBUTES);
	security.bInheritHandle = TRUE;
	security.nLength = sizeof SECURITY_ATTRIBUTES;
	
	creationDisposition = CModuleConfiguration::GetAppendToExistingLog(context) ? OPEN_ALWAYS : CREATE_ALWAYS;
	ErrorIf(INVALID_HANDLE_VALUE == (this->startupInfo.hStdOutput = CreateFileW(
		logName,
		GENERIC_READ | FILE_APPEND_DATA,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		&security,
		creationDisposition,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
		NULL)), 
		GetLastError());

	ErrorIf(0 == DuplicateHandle(
		GetCurrentProcess(),
		this->startupInfo.hStdOutput, 
		GetCurrentProcess(),
		&this->startupInfo.hStdError,
		0,
		TRUE,
		DUPLICATE_SAME_ACCESS),
		GetLastError());

	delete[] logName;
	logName = NULL;

	return S_OK;
Error:

	if (NULL != logName)
	{
		delete [] logName;
		logName = NULL;
	}

	if (INVALID_HANDLE_VALUE != this->startupInfo.hStdOutput)
	{
		CloseHandle(this->startupInfo.hStdOutput);
		this->startupInfo.hStdOutput = INVALID_HANDLE_VALUE;
	}

	if (INVALID_HANDLE_VALUE != this->startupInfo.hStdError)
	{
		CloseHandle(this->startupInfo.hStdError);
		this->startupInfo.hStdError = INVALID_HANDLE_VALUE;
	}

	return hr;
}