#include "precomp.h"

CNodeProcess::CNodeProcess(CNodeProcessManager* processManager, IHttpContext* context)
	: processManager(processManager), process(NULL), processWatcher(NULL), isClosing(FALSE),
	hasProcessExited(FALSE)
{
	RtlZeroMemory(this->namedPipe, sizeof this->namedPipe);
	RtlZeroMemory(&this->startupInfo, sizeof this->startupInfo);
	this->maxConcurrentRequestsPerProcess = CModuleConfiguration::GetMaxConcurrentRequestsPerProcess(context);
}

CNodeProcess::~CNodeProcess()
{
	this->isClosing = TRUE;

	if (NULL != this->process)
	{
		TerminateProcess(this->process, 2);
		CloseHandle(this->process);
		this->process = NULL;
	}

	if (NULL != this->processWatcher)
	{
		// The following check prevents a dead-lock between process watcher thread calling OnProcessExited 
		// which results in CNodeProcess::~ctor being called, 
		// and the wait for process watcher thread to exit in CNodeProcess::~ctor itself. 

		if (!this->hasProcessExited)
		{
			WaitForSingleObject(this->processWatcher, INFINITE);
		}
		CloseHandle(this->processWatcher);
		this->processWatcher = NULL;
	}
}

BOOL CNodeProcess::HasProcessExited()
{
	return this->hasProcessExited;
}

HRESULT CNodeProcess::Initialize(IHttpContext* context)
{
	HRESULT hr;
	UUID uuid;
	RPC_CSTR suuid = NULL;
	LPWSTR fullCommandLine = NULL;
	LPCWSTR coreCommandLine;
	LPCWSTR interceptor;
	PCWSTR scriptName;
	PROCESS_INFORMATION processInformation;
	DWORD exitCode = S_OK;
	LPCH newEnvironment = NULL;
	DWORD flags;
	HANDLE job;
	PWSTR currentDirectory = NULL;
	PWSTR scriptTranslated = NULL;
	DWORD currentDirectorySize = 0;
	CNodeApplication* app = this->GetProcessManager()->GetApplication();

	RtlZeroMemory(&processInformation, sizeof processInformation);
	RtlZeroMemory(&startupInfo, sizeof startupInfo);

	// initialize connection pool

	CheckError(this->connectionPool.Initialize(context));

	// generate the name for the named pipe to communicate with the node.js process
	
	ErrorIf(RPC_S_OK != UuidCreate(&uuid), ERROR_CAN_NOT_COMPLETE);
	ErrorIf(RPC_S_OK != UuidToString(&uuid, &suuid), ERROR_NOT_ENOUGH_MEMORY);
	_tcscpy(this->namedPipe, _T("\\\\.\\pipe\\"));
	_tcscpy(this->namedPipe + 9, (char*)suuid);
	RpcStringFree(&suuid);
	suuid = NULL;

	// build the full command line for the node.js process

	interceptor = CModuleConfiguration::GetInterceptor(context);
	coreCommandLine = CModuleConfiguration::GetNodeProcessCommandLine(context);
	scriptName = this->GetProcessManager()->GetApplication()->GetScriptName();
	// allocate memory for command line to allow for debugging options plus interceptor plus spaces and enclosing the script name in quotes
	ErrorIf(NULL == (fullCommandLine = new WCHAR[wcslen(coreCommandLine) + wcslen(interceptor) + wcslen(scriptName) + 256]), ERROR_NOT_ENOUGH_MEMORY); 
	wcscpy(fullCommandLine, coreCommandLine);

	// add debug options
	if (app->IsDebuggee())
	{
		WCHAR buffer[64];

		if (ND_DEBUG_BRK == app->GetDebugCommand())
		{
			swprintf(buffer, L" --debug-brk=%d ", app->GetDebugPort());					
		}
		else if (ND_DEBUG == app->GetDebugCommand())
		{
			swprintf(buffer, L" --debug=%d ", app->GetDebugPort());	
		}
		else
		{
			CheckError(ERROR_INVALID_PARAMETER);
		}

		wcscat(fullCommandLine, buffer);	
	}
	
	if (!app->IsDebugger()) 
	{
		// add interceptor
		wcscat(fullCommandLine, L" ");
		wcscat(fullCommandLine, interceptor);
	}

	// add application entry point
	wcscat(fullCommandLine, L" \"");
	wcscat(fullCommandLine, scriptName);
	wcscat(fullCommandLine, L"\"");

	// create the environment block for the node.js process 	

	CheckError(CModuleConfiguration::CreateNodeEnvironment(
		context, 
		app->IsDebugger() ? app->GetDebugPort() : 0, 
		this->namedPipe, 
		&newEnvironment));

	// establish the current directory for node.exe process as the value of the 'nodeProcessWorkingDirectory' configuration option.
	// if option is not set, establish the current directory to be the same as the location of the application *.js file
	// (in case of the debugger process, it is still the debuggee application file)

	LPCWSTR nodeWorkingDirectory = CModuleConfiguration::GetNodeProcessWorkingDirectory(context);
	if (wcslen(nodeWorkingDirectory) == 0)
	{
		scriptTranslated = (PWSTR)context->GetScriptTranslated(&currentDirectorySize);
		while (currentDirectorySize && scriptTranslated[currentDirectorySize] != L'\\' && scriptTranslated[currentDirectorySize] != L'/')
			currentDirectorySize--;
		ErrorIf(NULL == (currentDirectory = new WCHAR[wcslen(scriptTranslated) + 1]), ERROR_NOT_ENOUGH_MEMORY);
		wcscpy(currentDirectory, scriptTranslated);
		currentDirectory[currentDirectorySize] = L'\0';
	}
	else
	{
		currentDirectorySize = wcslen(nodeWorkingDirectory);
		ErrorIf(NULL == (currentDirectory = new WCHAR[currentDirectorySize + 1]), ERROR_NOT_ENOUGH_MEMORY);
		wcscpy(currentDirectory, nodeWorkingDirectory);
		currentDirectory[currentDirectorySize] = L'\0';
	}

	// create startup info for the node.js process

	RtlZeroMemory(&this->startupInfo, sizeof this->startupInfo);
	GetStartupInfoW(&startupInfo);
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
	
	if(!CreateProcessW(
			NULL,
			fullCommandLine,
			NULL,
			NULL,
			TRUE,
			flags,
			newEnvironment,
			currentDirectory,
			&this->startupInfo,
			&processInformation)) 
	{
		if (ERROR_FILE_NOT_FOUND == (hr = GetLastError()))
		{
			hr = IISNODE_ERROR_UNABLE_TO_START_NODE_EXE;
		}

		CheckError(hr);
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
	this->pid = processInformation.dwProcessId;
	
	// start process watcher thread to get notified of premature node process termination in CNodeProcess::OnProcessExited		

	ResumeThread(this->processWatcher);

	// clean up

	delete [] currentDirectory;
	currentDirectory = NULL;
	delete [] newEnvironment;
	newEnvironment = NULL;
	delete [] fullCommandLine;
	fullCommandLine = NULL;
	CloseHandle(processInformation.hThread);
	processInformation.hThread = NULL;	

	if (this->GetProcessManager()->GetApplication()->IsDebugger())
	{
		this->GetProcessManager()->GetEventProvider()->Log(
			L"iisnode initialized a new node.exe debugger process", WINEVENT_LEVEL_INFO);
	}
	else
	{
		this->GetProcessManager()->GetEventProvider()->Log(
			L"iisnode initialized a new node.exe process", WINEVENT_LEVEL_INFO);
	}

	return S_OK;
Error:

	if (this->GetProcessManager()->GetApplication()->IsDebugger())
	{
		this->GetProcessManager()->GetEventProvider()->Log(
			L"iisnode failed to initialize a new node.exe debugger process", WINEVENT_LEVEL_ERROR);
	}
	else
	{
		this->GetProcessManager()->GetEventProvider()->Log(
			L"iisnode failed to initialize a new node.exe process", WINEVENT_LEVEL_ERROR);
	}

	if (currentDirectory)
	{
		delete [] currentDirectory;
		currentDirectory = NULL;
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

unsigned int WINAPI CNodeProcess::ProcessWatcher(void* arg)
{
	CNodeProcess* process = (CNodeProcess*)arg;
	DWORD exitCode;
	DWORD waitResult;
	CNodeEventProvider* log = process->GetProcessManager()->GetEventProvider();
	BOOL isDebugger = process->GetProcessManager()->GetApplication()->IsDebugger();

	while (!process->isClosing && process->process)
	{
		waitResult = WaitForSingleObjectEx(process->process, INFINITE, TRUE);
		if (GetExitCodeProcess(process->process, &exitCode) && STILL_ACTIVE != exitCode)
		{
			if (isDebugger)
			{
				log->Log(L"iisnode detected termination of node.exe debugger process", WINEVENT_LEVEL_ERROR);
			}
			else
			{
				log->Log(L"iisnode detected termination of node.exe process", WINEVENT_LEVEL_ERROR);
			}

			if (!process->isClosing)
			{
				process->OnProcessExited();
			}

			return exitCode;
		}
	}

	return S_OK;
}

CNodeProcessManager* CNodeProcess::GetProcessManager()
{
	return this->processManager;
}

HANDLE CNodeProcess::GetProcess()
{
	return this->process;
}

DWORD CNodeProcess::GetPID()
{
	return this->pid;
}

DWORD CNodeProcess::GetActiveRequestCount()
{
	return this->activeRequestPool.GetRequestCount();
}

HRESULT CNodeProcess::AcceptRequest(CNodeHttpStoredContext* context)
{
	HRESULT hr;

	CheckError(this->activeRequestPool.Add(context));
	context->SetNodeProcess(this);
	CheckError(CProtocolBridge::InitiateRequest(context));

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
	this->activeRequestPool.Remove(); // this call may results in "this" being disposed on a different thread
}

void CNodeProcess::OnProcessExited()
{	
	this->isClosing = TRUE;
	this->hasProcessExited = TRUE;
	this->GetProcessManager()->RecycleProcess(this);
}

void CNodeProcess::SignalWhenDrained(HANDLE handle)
{	
	this->activeRequestPool.SignalWhenDrained(handle);
}

HRESULT CNodeProcess::CreateStdHandles(IHttpContext* context)
{
	this->startupInfo.hStdError = this->startupInfo.hStdInput = this->startupInfo.hStdOutput = INVALID_HANDLE_VALUE;
	this->startupInfo.dwFlags = STARTF_USESTDHANDLES;

	HRESULT hr;
	SECURITY_ATTRIBUTES security;

	// stdout == stderr

	RtlZeroMemory(&security, sizeof SECURITY_ATTRIBUTES);
	security.bInheritHandle = TRUE;
	security.nLength = sizeof SECURITY_ATTRIBUTES;
	
	ErrorIf(INVALID_HANDLE_VALUE == (this->startupInfo.hStdOutput = CreateFileW(
		L"NUL",
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		&security,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
		NULL)), GetLastError());

	ErrorIf(0 == DuplicateHandle(
		GetCurrentProcess(),
		this->startupInfo.hStdOutput, 
		GetCurrentProcess(),
		&this->startupInfo.hStdError,
		0,
		TRUE,
		DUPLICATE_SAME_ACCESS),
		GetLastError());

	return S_OK;
Error:

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

CConnectionPool* CNodeProcess::GetConnectionPool()
{
	return &this->connectionPool;
}

char* CNodeProcess::TryGetLog(IHttpContext* context, DWORD* size)
{
	HRESULT hr;
	HANDLE file = INVALID_HANDLE_VALUE;
	char* log = NULL;
	*size = 0;
	PWSTR currentDirectory;
	DWORD currentDirectorySize;
	PWSTR logRelativeDirectory;
	DWORD logRelativeDirectoryLength;
	PWSTR logName;
	HANDLE findHandle = INVALID_HANDLE_VALUE;
	WIN32_FIND_DATAW findData;
	WCHAR tmp[64];
	DWORD computerNameSize;

	// establish the log file directory name by composing the directory 
	// of the script with the relative log directory name

	currentDirectory = (PWSTR)context->GetScriptTranslated(&currentDirectorySize);
	while (currentDirectorySize && currentDirectory[currentDirectorySize] != L'\\' && currentDirectory[currentDirectorySize] != L'/')
		currentDirectorySize--;
	logRelativeDirectory = CModuleConfiguration::GetLogDirectory(context);
	logRelativeDirectoryLength = wcslen(logRelativeDirectory);
	ErrorIf(NULL == (logName = (WCHAR*)context->AllocateRequestMemory((currentDirectorySize + logRelativeDirectoryLength + 256) * sizeof WCHAR)),
		ERROR_NOT_ENOUGH_MEMORY);
	wcsncpy(logName, currentDirectory, currentDirectorySize + 1);
	logName[currentDirectorySize + 1] = L'\0';
	wcscat(logName, logRelativeDirectory);
	if (logRelativeDirectoryLength && logRelativeDirectory[logRelativeDirectoryLength - 1] != L'\\') 
	{
		wcscat(logName, L"\\");
	}
	currentDirectorySize = wcslen(logName);

	// construct a wildcard stderr log file name from computer name and PID

	computerNameSize = GetEnvironmentVariableW(L"COMPUTERNAME", tmp, 64);
	ErrorIf(0 == computerNameSize, GetLastError());
	ErrorIf(64 < computerNameSize, E_FAIL);
	wcscat(logName, tmp);
	swprintf(tmp, L"-%d-stderr-*.txt", this->pid);
	wcscat(logName, tmp);
	
	// find a file matching the wildcard name

	ErrorIf(INVALID_HANDLE_VALUE == (findHandle = FindFirstFileW(logName, &findData)), GetLastError());
	FindClose(findHandle);
	findHandle = INVALID_HANDLE_VALUE;

	// construct the actual log file name to read

	logName[currentDirectorySize] = L'\0';
	wcscat(logName, findData.cFileName);

	// read the log file

	ErrorIf(INVALID_HANDLE_VALUE == (file = CreateFileW(
		logName,
		GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, 
		NULL,
		OPEN_EXISTING,
		0,
		NULL)), GetLastError());

	ErrorIf(INVALID_FILE_SIZE == (*size = GetFileSize(file, NULL)), GetLastError());

	if (*size > 65536)
	{
		// if log is larger than 64k, return only the last 64k

		*size = 65536;
		ErrorIf(INVALID_SET_FILE_POINTER == SetFilePointer(file, *size, NULL, FILE_END), GetLastError());
	}

	ErrorIf(NULL == (log = (char*)context->AllocateRequestMemory(*size)), ERROR_NOT_ENOUGH_MEMORY);
	ErrorIf(0 == ReadFile(file, log, *size, size, NULL), GetLastError());

	CloseHandle(file);
	file = INVALID_HANDLE_VALUE;

	return log;

Error:

	if (INVALID_HANDLE_VALUE != file)
	{
		CloseHandle(file);
		file = INVALID_HANDLE_VALUE;
	}

	if (INVALID_HANDLE_VALUE != findHandle) 
	{
		FindClose(findHandle);
		findHandle = INVALID_HANDLE_VALUE;
	}

	// log does not need to be freed - IIS will take care of it when IHttpContext is disposed

	*size = 0;

	return NULL;
}
