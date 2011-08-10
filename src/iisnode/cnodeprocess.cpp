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
	TCHAR environment[256 + 5 + 1 + 1]; // max named pipe name, "PORT=", terminating null, and an extra block terminating null
	STARTUPINFO startupInfo;
	PROCESS_INFORMATION processInformation;

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

	RtlZeroMemory(environment, sizeof environment);
	_tcscpy(environment, _T("PORT="));
	_tcscat(environment, this->namedPipe);

	// create startup info for the node.js process

	GetStartupInfo(&startupInfo);
	// TODO, tjanczuk, capture stderr and stdout of the newly created node.js process
	startupInfo.dwFlags = STARTF_USESTDHANDLES;
	startupInfo.hStdError = startupInfo.hStdInput = startupInfo.hStdOutput = NULL;

	// create the node.js process

	RtlZeroMemory(&processInformation, sizeof processInformation);
	ErrorIf(FALSE == CreateProcess(
			NULL,
			fullCommandLine,
			NULL,
			NULL,
			FALSE,
			CREATE_NO_WINDOW | DETACHED_PROCESS,
			environment,
			NULL,
			&startupInfo,
			&processInformation
		), GetLastError());

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

	return hr;
}

CNodeProcessManager* CNodeProcess::GetProcessManager()
{
	return this->processManager;
}

HRESULT CNodeProcess::AcceptRequest(CNodeHttpStoredContext* context)
{
	HRESULT hr;

	if (S_OK == (hr = this->activeRequestPool.Add(context)))
	{
		context->SetNodeProcess(this);
		CheckError(CProtocolBridge::InitiateRequest(context));
	}

	return S_OK;
Error:
	
	context->SetNodeProcess(NULL);
	this->activeRequestPool.Remove(context);

	return hr;
}

LPCTSTR CNodeProcess::GetNamedPipeName()
{
	return this->namedPipe;
}