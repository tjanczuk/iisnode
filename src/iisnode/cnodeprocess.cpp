#include "precomp.h"

CNodeProcess::CNodeProcess(CNodeProcessManager* processManager)
	: processManager(processManager), process(NULL)
{
	RtlZeroMemory(this->namedPipe, sizeof this->namedPipe);
	this->maxConcurrentRequestsPerProcess = CModuleConfiguration::GetMaxConcurrentRequestsPerProcess();
}

CNodeProcess::~CNodeProcess()
{
	if (NULL != this->process)
	{
		TerminateProcess(this->process, 2);
		CloseHandle(this->process);
		this->process = NULL;
	}
}

HRESULT CNodeProcess::Initialize()
{
	HRESULT hr;
	UUID uuid;
	RPC_CSTR suuid = NULL;
	LPTSTR fullCommandLine = NULL;
	LPCTSTR coreCommandLine;
	PCWSTR scriptName;
	size_t coreCommandLineLength, scriptNameLength, scriptNameLengthW;
	STARTUPINFO startupInfo;
	PROCESS_INFORMATION processInformation;
	DWORD exitCode = S_OK;
	LPCH currentEnvironment = NULL;
	LPCH newEnvironment = NULL;
	DWORD environmentSize;
	DWORD flags;
	HANDLE job;

	// generate the name for the named pipe to communicate with the node.js process
	
	ErrorIf(RPC_S_OK != UuidCreate(&uuid), ERROR_CAN_NOT_COMPLETE);
	ErrorIf(RPC_S_OK != UuidToString(&uuid, &suuid), ERROR_NOT_ENOUGH_MEMORY);
	_tcscpy(this->namedPipe, _T("\\\\.\\pipe\\"));
	_tcscpy(this->namedPipe + 9, (char*)suuid);
	RpcStringFree(&suuid);
	suuid = NULL;

	// build the full command line for the node.js process

	coreCommandLine = CModuleConfiguration::GetNodeProcessCommandLine();
	scriptName = this->GetProcessManager()->GetApplication()->GetScriptName();
	coreCommandLineLength = _tcslen(coreCommandLine);
	scriptNameLengthW = wcslen(scriptName) + 1;
	ErrorIf(0 != wcstombs_s(&scriptNameLength, NULL, 0, scriptName, _TRUNCATE), ERROR_CAN_NOT_COMPLETE);
	ErrorIf(NULL == (fullCommandLine = new TCHAR[coreCommandLineLength + scriptNameLength + 1]), ERROR_NOT_ENOUGH_MEMORY);
	_tcscpy(fullCommandLine, coreCommandLine);
	_tcscat(fullCommandLine, _T(" "));
	ErrorIf(0 != wcstombs_s(&scriptNameLength, fullCommandLine + coreCommandLineLength + 1, scriptNameLength, scriptName, _TRUNCATE), ERROR_CAN_NOT_COMPLETE);

	// create the environment block for the node.js process - pass in the named pipe name; 
	// this is a zero terminated list of zero terminated strings of the form <var>=<value>

	ErrorIf(NULL == (currentEnvironment = GetEnvironmentStrings()), GetLastError());
	environmentSize = 0;
	do {
		while (*(currentEnvironment + environmentSize++) != 0);
	} while (*(currentEnvironment + environmentSize++) != 0);
	ErrorIf(NULL == (newEnvironment = (LPCH)new char[environmentSize + 256 + 1 + 1]), ERROR_NOT_ENOUGH_MEMORY);	
	_tcscpy(newEnvironment, _T("PORT="));
	_tcscat(newEnvironment, this->namedPipe);
	memcpy(newEnvironment + 6 + strlen(this->namedPipe), currentEnvironment, environmentSize);
	FreeEnvironmentStrings(currentEnvironment);
	currentEnvironment = NULL;

	// create startup info for the node.js process

	GetStartupInfo(&startupInfo);
	// TODO, tjanczuk, capture stderr and stdout of the newly created node.js process
	startupInfo.dwFlags = 0; //STARTF_USESTDHANDLES;
	startupInfo.hStdError = startupInfo.hStdInput = startupInfo.hStdOutput = INVALID_HANDLE_VALUE;

	// create the node.exe process

	flags = DETACHED_PROCESS | CREATE_SUSPENDED;
	if (this->GetProcessManager()->GetApplication()->GetApplicationManager()->GetBreakAwayFromJobObject())
	{
		flags |= CREATE_BREAKAWAY_FROM_JOB;
	}

	RtlZeroMemory(&processInformation, sizeof processInformation);
	ErrorIf(FALSE == CreateProcess(
			NULL,
			fullCommandLine,
			NULL,
			NULL,
			TRUE,
			flags,
			newEnvironment,
			NULL,
			&startupInfo,
			&processInformation
		), GetLastError());

	// join a job object if needed, then resume the process

	job = this->GetProcessManager()->GetApplication()->GetApplicationManager()->GetJobObject();
	if (NULL != job)
	{
		ErrorIf(!AssignProcessToJobObject(job, processInformation.hProcess), HRESULT_FROM_WIN32(GetLastError()));
	}

	ErrorIf((DWORD) -1 == ResumeThread(processInformation.hThread), GetLastError());
	ErrorIf(GetExitCodeProcess(processInformation.hProcess, &exitCode) && STILL_ACTIVE != exitCode, exitCode);

	// clean up

	delete [] newEnvironment;
	newEnvironment = NULL;
	delete [] fullCommandLine;
	fullCommandLine = NULL;
	CloseHandle(processInformation.hThread);
	processInformation.hThread = NULL;

	this->process = processInformation.hProcess;

	return S_OK;
Error:

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

	if (NULL != currentEnvironment)
	{
		FreeEnvironmentStrings(currentEnvironment);
		currentEnvironment = NULL;
	}

	if (NULL != newEnvironment)
	{
		delete[] newEnvironment;
		newEnvironment = NULL;
	}

	return hr;
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
	this->activeRequestPool.Remove(context);
	this->GetProcessManager()->GetApplication()->GetApplicationManager()->GetAsyncManager()->PostContinuation(CNodeProcessManager::TryDispatchOneRequest, this->GetProcessManager());
}