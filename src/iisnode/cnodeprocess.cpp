#include "precomp.h"

CNodeProcess::CNodeProcess(CNodeProcessManager* processManager, IHttpContext* context, DWORD ordinal)
	: processManager(processManager), process(NULL), processWatcher(NULL), isClosing(FALSE), ordinal(ordinal),
	hasProcessExited(FALSE), truncatePending(FALSE)
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
	CNodeApplication* app = this->GetProcessManager()->GetApplication();

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
	// allocate memory for command line to allow for debugging options plus enclosing the script name in quotes
	ErrorIf(NULL == (fullCommandLine = new TCHAR[coreCommandLineLength + scriptNameLength + 256]), ERROR_NOT_ENOUGH_MEMORY); 
	_tcscpy(fullCommandLine, coreCommandLine);
	DWORD offset;
	if (app->IsDebuggee())
	{
		char buffer[64];

		if (ND_DEBUG_BRK == app->GetDebugCommand())
		{
			sprintf(buffer, " --debug-brk=%d \"", app->GetDebugPort());					
		}
		else if (ND_DEBUG == app->GetDebugCommand())
		{
			sprintf(buffer, " --debug=%d \"", app->GetDebugPort());	
		}
		else
		{
			CheckError(ERROR_INVALID_PARAMETER);
		}

		_tcscat(fullCommandLine, buffer);	
		offset = strlen(buffer);
	}
	else 
	{
		_tcscat(fullCommandLine, _T(" \""));
		offset = 2;
	}	
	ErrorIf(0 != wcstombs_s(&scriptNameLength, fullCommandLine + coreCommandLineLength + offset, scriptNameLength, scriptName, _TRUNCATE), ERROR_CAN_NOT_COMPLETE);	
	_tcscat(fullCommandLine, _T("\""));

	// create the environment block for the node.js process 	

	CheckError(CModuleConfiguration::CreateNodeEnvironment(
		context, 
		app->IsDebugger() ? app->GetDebugPort() : 0, 
		this->namedPipe, 
		&newEnvironment));

	// establish the current directory for node.exe process to be the same as the location of the application *.js file
	// (in case of the debugger process, it is still the debuggee application file)

	currentDirectory = (PWSTR)context->GetScriptTranslated(&currentDirectorySize);
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
	
	if(!CreateProcess(
			NULL,
			fullCommandLine,
			NULL,
			NULL,
			TRUE,
			flags,
			newEnvironment,
			currentDirectoryA,
			&this->startupInfo,
			&processInformation)) 
	{
		if (ERROR_FILE_NOT_FOUND == (hr = GetLastError()))
		{
			hr = IISNODE_ERROR_UNABLE_TO_START_NODE_EXE;
		}

		CheckError(hr);
	}
		

	// duplicate stdout and stderr handles to allow flushing without regard for whether the node process exited
	// closing of the original handles will be taken care of by the newly started process

	ErrorIf(0 == DuplicateHandle(
		GetCurrentProcess(), 
		this->startupInfo.hStdOutput, 
		GetCurrentProcess(), 
		&this->startupInfo.hStdOutput, 
		0, 
		TRUE, 
		DUPLICATE_SAME_ACCESS),
		GetLastError());

	ErrorIf(0 == DuplicateHandle(
		GetCurrentProcess(), 
		this->startupInfo.hStdError, 
		GetCurrentProcess(), 
		&this->startupInfo.hStdError, 
		0, 
		TRUE, 
		DUPLICATE_SAME_ACCESS),
		GetLastError());

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
	if (this->loggingEnabled)
	{
		const char* truncateMessage = ">>>> iisnode truncated the log file because it exceeded the configured maximum size\n";
		LARGE_INTEGER fileSize;

		// flush the file

		FlushFileBuffers(this->startupInfo.hStdOutput);

		// truncate the log file back to 0 if the max size is exceeded

		if (!this->truncatePending && GetFileSizeEx(this->startupInfo.hStdOutput, &fileSize))
		{
			if (fileSize.QuadPart > this->maxLogSizeInBytes)
			{			
				RtlZeroMemory(&this->overlapped, sizeof this->overlapped); // this also sets the Offset and OffsetHigh to 0		
				this->overlapped.hEvent = this;
				this->truncatePending = TRUE; // this will be reset to FALSE in the completion routine of WriteFileEx below
				if (!WriteFileEx(
					this->startupInfo.hStdOutput, 
					(void*)truncateMessage, 
					strlen(truncateMessage), 
					&this->overlapped, 
					CNodeProcess::TruncateLogFileCompleted)) 
				{
					this->truncatePending = FALSE;
				}
			}
		}
	}
}

void CALLBACK CNodeProcess::TruncateLogFileCompleted(DWORD dwErrorCode, DWORD dwNumberOfBytesTransfered, LPOVERLAPPED lpOverlapped)
{
	CNodeProcess* process = (CNodeProcess*)lpOverlapped->hEvent;
	process->truncatePending = FALSE;
	SetEndOfFile(process->startupInfo.hStdOutput);
	FlushFileBuffers(process->startupInfo.hStdOutput);
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
		waitResult = WaitForSingleObjectEx(process->process, process->loggingEnabled ? process->logFlushInterval : INFINITE, TRUE);
		if (WAIT_TIMEOUT == waitResult)
		{
			process->FlushStdHandles();

			if (isDebugger)
			{
				log->Log(L"iisnode flushed standard handles of the node.exe debugger process", WINEVENT_LEVEL_VERBOSE);
			}
			else
			{
				log->Log(L"iisnode flushed standard handles of the node.exe process", WINEVENT_LEVEL_VERBOSE);
			}
		}
		else if (GetExitCodeProcess(process->process, &exitCode) && STILL_ACTIVE != exitCode)
		{
			process->FlushStdHandles();

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
	CNodeProcessManager* processManager = this->GetProcessManager();
	BOOL isClosing = this->isClosing; // this is just an optimization to save on context switch
	
	this->activeRequestPool.Remove(); // this call may results in "this" being disposed on a different thread
	
	if (!isClosing)
	{
		processManager->PostDispatchOneRequest();
	}
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
	WCHAR* logName = NULL;
	SECURITY_ATTRIBUTES security;
	DWORD creationDisposition;

	if (this->loggingEnabled)
	{
		PCWSTR scriptName;
		LPWSTR logComponentName = CModuleConfiguration::GetLogDirectoryNameSuffix(context);
		CheckNull(logComponentName);	

		// allocate enough memory to store log file name of the form <scriptName>.<logComponentName>\<ordinalProcessNumber>.txt

		scriptName = this->GetProcessManager()->GetApplication()->GetScriptName();
		ErrorIf(NULL == (logName = new WCHAR[wcslen(scriptName) + 1 + wcslen(logComponentName) + 10 + 4 + 1]), ERROR_NOT_ENOUGH_MEMORY); 

		// ensure a directory for storing the log file exists; the directory name is of the form <scriptName>.<logComponentName>

		swprintf(logName, L"%s.%s", scriptName, logComponentName);
		if (!CreateDirectoryW(logName, NULL))
		{
			hr = GetLastError();
			if (ERROR_ALREADY_EXISTS != hr)
			{
				this->GetProcessManager()->GetEventProvider()->Log(
					L"iisnode failed to create directory to store log files for the node.exe process", WINEVENT_LEVEL_ERROR);

				hr = IISNODE_ERROR_UNABLE_TO_CREATE_LOG_FILE;
				CheckError(hr);
			}
		}

		// create log file name

		swprintf(logName, L"%s.%s\\%d.txt", scriptName, logComponentName, this->ordinal);		

		creationDisposition = CModuleConfiguration::GetAppendToExistingLog(context) ? OPEN_ALWAYS : CREATE_ALWAYS;
	}
	else
	{
		creationDisposition = OPEN_EXISTING;
	}

	// stdout == stderr

	RtlZeroMemory(&security, sizeof SECURITY_ATTRIBUTES);
	security.bInheritHandle = TRUE;
	security.nLength = sizeof SECURITY_ATTRIBUTES;
	
	if (INVALID_HANDLE_VALUE == (this->startupInfo.hStdOutput = CreateFileW(
		this->loggingEnabled ? logName : L"NUL",
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		&security,
		creationDisposition,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
		NULL)))
	{
		hr = this->loggingEnabled ? IISNODE_ERROR_UNABLE_TO_CREATE_LOG_FILE : GetLastError();
		CheckError(hr);
	}

	ErrorIf(0 == DuplicateHandle(
		GetCurrentProcess(),
		this->startupInfo.hStdOutput, 
		GetCurrentProcess(),
		&this->startupInfo.hStdError,
		0,
		TRUE,
		DUPLICATE_SAME_ACCESS),
		GetLastError());

	if (NULL != logName)
	{
		delete [] logName;
		logName = NULL;
	}

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