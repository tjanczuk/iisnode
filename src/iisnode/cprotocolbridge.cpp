#include "precomp.h"

HRESULT CProtocolBridge::PostponeProcessing(CNodeHttpStoredContext* context, DWORD dueTime)
{
	CAsyncManager* async = context->GetNodeApplication()->GetApplicationManager()->GetAsyncManager();
	LARGE_INTEGER delay;
	delay.QuadPart = dueTime;
	delay.QuadPart *= -10000; // convert from ms to 100ns units

	return async->SetTimer(context->GetAsyncContext(), &delay);
}

BOOL CProtocolBridge::SendIisnodeError(IHttpContext* httpCtx, HRESULT hr)
{
	if (!CModuleConfiguration::GetDevErrorsEnabled(httpCtx))
	{
		return FALSE;
	}

	switch (hr) {

	default: 
		return FALSE;

	case IISNODE_ERROR_UNRECOGNIZED_DEBUG_COMMAND:
		CProtocolBridge::SendSyncResponse(
			httpCtx, 
			200, 
			"OK", 
			hr, 
			TRUE, 
			"Unrecognized debugging command. Supported commands are ?debug (default), ?brk, and ?kill.");
		break;

	case IISNODE_ERROR_UNABLE_TO_FIND_DEBUGGING_PORT:
		CProtocolBridge::SendSyncResponse(
			httpCtx, 
			200, 
			"OK", 
			hr, 
			TRUE, 
			"The debugger was unable to acquire a TCP port to establish communication with the debugee. "
			"This may be due to lack of free TCP ports in the range specified in the <a href=""https://github.com/tjanczuk/iisnode/blob/master/src/samples/configuration/web.config"">"
			"system.webServer/iisnode/@debuggerPortRange</a> configuration "
			"section, or due to lack of permissions to create TCP listeners by the identity of the IIS worker process.");
		break;

	case IISNODE_ERROR_UNABLE_TO_CONNECT_TO_DEBUGEE:
		CProtocolBridge::SendSyncResponse(
			httpCtx, 
			200, 
			"OK", 
			hr, 
			TRUE, 
			"The debugger was unable to connect to the the debugee. "
			"This may be due to the debugee process terminating during startup (e.g. due to an unhandled exception) or "
			"failing to establish a TCP listener on the debugging port. ");
		break;

	case IISNODE_ERROR_INSPECTOR_NOT_FOUND:
		CProtocolBridge::SendSyncResponse(
			httpCtx, 
			200, 
			"OK", 
			hr, 
			TRUE, 
			"The version of iisnode installed on the server does not support remote debugging. "
			"To use remote debugging, please update your iisnode installation on the server to one available from "
			"<a href=""http://github.com/tjanczuk/iisnode/downloads"">http://github.com/tjanczuk/iisnode/downloads</a>. "
			"We apologize for inconvenience.");
		break;

	case IISNODE_ERROR_UNABLE_TO_CREATE_LOG_FILE:
		CProtocolBridge::SendSyncResponse(
			httpCtx, 
			200, 
			"OK", 
			hr, 
			TRUE, 
			"The iisnode module is unable to create a log file to capture stdout and stderr output from node.exe. "
			"Please check that the identity of the IIS application pool running the node.js application has read and write access "
			"permissions to the directory on the server where the node.js application is located. Alternatively you can disable logging "
			"by setting <a href=""https://github.com/tjanczuk/iisnode/blob/master/src/samples/configuration/web.config"">"
			"system.webServer/iisnode/@loggingEnabled</a> element of web.config to 'false'.");
		break;

	case IISNODE_ERROR_UNABLE_TO_CREATE_DEBUGGER_FILES:
		CProtocolBridge::SendSyncResponse(
			httpCtx, 
			200, 
			"OK", 
			hr, 
			TRUE, 
			"The iisnode module is unable to deploy supporting files necessary to initialize the debugger. "
			"Please check that the identity of the IIS application pool running the node.js application has read and write access "
			"permissions to the directory on the server where the node.js application is located.");
		break;

	case IISNODE_ERROR_UNABLE_TO_START_NODE_EXE:
		LPCTSTR commandLine = CModuleConfiguration::GetNodeProcessCommandLine(httpCtx);
		char* errorMessage;
		if (NULL == (errorMessage = (char*)httpCtx->AllocateRequestMemory(strlen(commandLine) + 512)))
		{
			errorMessage = 
				"The iisnode module is unable to start the node.exe process. Make sure the node.exe executable is available "
				"at the location specified in the <a href=""https://github.com/tjanczuk/iisnode/blob/master/src/samples/configuration/web.config"">"
				"system.webServer/iisnode/@nodeProcessCommandLine</a> element of web.config. "
				"By default node.exe is expected to be installed in %ProgramFiles%\\nodejs folder on x86 systems and "
				"%ProgramFiles(x86)%\\nodejs folder on x64 systems.";
		}
		else
		{
			sprintf(errorMessage, 
				"The iisnode module is unable to start the node.exe process. Make sure the node.exe executable is available "
				"at the location specified in the <a href=""https://github.com/tjanczuk/iisnode/blob/master/src/samples/configuration/web.config"">"
				"system.webServer/iisnode/@nodeProcessCommandLine</a> element of web.config. "
				"The command line iisnode attempted to run was:<br><br>%s", commandLine);
		}

		CProtocolBridge::SendSyncResponse(
			httpCtx, 
			200, 
			"OK", 
			hr, 
			TRUE, 
			errorMessage);
		break;

	};

	return TRUE;
}

BOOL CProtocolBridge::SendIisnodeError(CNodeHttpStoredContext* ctx, HRESULT hr)
{
	if (CProtocolBridge::SendIisnodeError(ctx->GetHttpContext(), hr))
	{
		ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode request processing failed for reasons recognized by iisnode", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

		if (INVALID_HANDLE_VALUE != ctx->GetPipe())
		{
			CloseHandle(ctx->GetPipe());
			ctx->SetPipe(INVALID_HANDLE_VALUE);
		}

		CProtocolBridge::FinalizeResponseCore(
			ctx, 
			RQ_NOTIFICATION_FINISH_REQUEST, 
			hr, 
			ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider(),
			L"iisnode posts completion from SendIisnodeError", 
			WINEVENT_LEVEL_VERBOSE);

		return TRUE;
	}

	return FALSE;
}

HRESULT CProtocolBridge::SendEmptyResponse(CNodeHttpStoredContext* context, USHORT status, PCTSTR reason, HRESULT hresult, BOOL disableCache)
{
	context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode request processing failed for reasons unrecognized by iisnode", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());

	if (INVALID_HANDLE_VALUE != context->GetPipe())
	{
		CloseHandle(context->GetPipe());
		context->SetPipe(INVALID_HANDLE_VALUE);
	}

	CProtocolBridge::SendEmptyResponse(context->GetHttpContext(), status, reason, hresult, disableCache);	

	CProtocolBridge::FinalizeResponseCore(
		context, 
		RQ_NOTIFICATION_FINISH_REQUEST, 
		hresult, 
		context->GetNodeApplication()->GetApplicationManager()->GetEventProvider(),
		L"iisnode posts completion from SendEmtpyResponse", 
		WINEVENT_LEVEL_VERBOSE);

	return S_OK;
}

HRESULT CProtocolBridge::SendSyncResponse(IHttpContext* httpCtx, USHORT status, PCTSTR reason, HRESULT hresult, BOOL disableCache, PCSTR htmlBody)
{
	HRESULT hr;
	DWORD bytesSent;
	HTTP_DATA_CHUNK body;

	CProtocolBridge::SendEmptyResponse(httpCtx, status, reason, hresult, disableCache);

	IHttpResponse* response = httpCtx->GetResponse();
	response->SetHeader(HttpHeaderContentType, "text/html", 9, TRUE);
	body.DataChunkType = HttpDataChunkFromMemory;
	body.FromMemory.pBuffer = (PVOID)htmlBody;
	body.FromMemory.BufferLength = strlen(htmlBody);
	CheckError(response->WriteEntityChunks(&body, 1, FALSE, FALSE, &bytesSent));

	return S_OK;
Error:
	return hr;
}

void CProtocolBridge::SendEmptyResponse(IHttpContext* httpCtx, USHORT status, PCTSTR reason, HRESULT hresult, BOOL disableCache)
{
	if (!httpCtx->GetResponseHeadersSent())
	{
		httpCtx->GetResponse()->Clear();
		httpCtx->GetResponse()->SetStatus(status, reason, 0, hresult);
		if (disableCache)
		{
			httpCtx->GetResponse()->SetHeader(HttpHeaderCacheControl, "no-cache", 8, TRUE);
		}
	}
}

HRESULT CProtocolBridge::InitiateRequest(CNodeHttpStoredContext* context)
{
	HRESULT hr;
	BOOL requireChildContext = FALSE;	
	BOOL mainDebuggerPage = FALSE;
	IHttpContext* child = NULL;
	BOOL completionExpected;

	// determine what the target path of the request is

	if (context->GetNodeApplication()->IsDebugger())
	{
		// All debugger URLs require rewriting. Requests for static content will be processed using a child http context and served
		// by a static file handler. Debugging protocol requests will continue executing in the current context and be processed 
		// by the node-inspector application.

		CheckError(CNodeDebugger::DispatchDebuggingRequest(context, &requireChildContext, &mainDebuggerPage));
	}
	else
	{
		// For application requests, if the URL rewrite module had been used to rewrite the URL, 
		// present the original URL to the node.js application instead of the re-written one.

		PCSTR url;
		USHORT urlLength;
		IHttpRequest* request = context->GetHttpContext()->GetRequest();

		if (NULL == (url = request->GetHeader("X-Original-URL", &urlLength)))
		{
			HTTP_REQUEST* raw = request->GetRawHttpRequest();
			context->SetTargetUrl(raw->pRawUrl, raw->RawUrlLength);
		}
		else
		{
			context->SetTargetUrl(url, urlLength);
		}
	}

	// determine how to process the request

	if (requireChildContext)
	{
		CheckError(context->GetHttpContext()->CloneContext(CLONE_FLAG_BASICS | CLONE_FLAG_ENTITY | CLONE_FLAG_HEADERS, &child));
		if (mainDebuggerPage)
		{
			// Prevent client caching of responses to requests for debugger entry points e.g. app.js/debug or app.js/debug?brk
			// This is to allow us to run initialization logic on the server if necessary every time user refreshes the page
			// Static content subordinate to the main debugger page is eligible for client side caching

			child->GetResponse()->SetHeader("Cache-Control", "no-cache", 8, TRUE);
		}

		CheckError(child->GetRequest()->SetUrl(context->GetTargetUrl(), context->GetTargetUrlLength(), FALSE));
		context->SetChildContext(child);
		context->SetNextProcessor(CProtocolBridge::ChildContextCompleted);
		
		CheckError(context->GetHttpContext()->ExecuteRequest(TRUE, child, 0, NULL, &completionExpected));
		if (!completionExpected)
		{
			CProtocolBridge::ChildContextCompleted(S_OK, 0, context->GetOverlapped());
		}
	}
	else
	{
		context->SetNextProcessor(CProtocolBridge::CreateNamedPipeConnection);
		CProtocolBridge::CreateNamedPipeConnection(S_OK, 0, context->InitializeOverlapped());
	}

	return S_OK; 
Error:

	if (child)
	{		
		child->ReleaseClonedContext();
		child = NULL;
		context->SetChildContext(NULL);
	}

	return hr;
}

void WINAPI CProtocolBridge::ChildContextCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
	CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

	ctx->GetChildContext()->ReleaseClonedContext();
	ctx->SetChildContext(NULL);

	if (S_OK == error)
	{
		ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode finished processing child http request", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
	}
	else
	{
		ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode failed to process child http request", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
	}

	ctx->SetHresult(error);
	ctx->SetRequestNotificationStatus(RQ_NOTIFICATION_CONTINUE);	
	ctx->SetNextProcessor(NULL);
	
	CProtocolBridge::FinalizeResponseCore(
		ctx, 
		RQ_NOTIFICATION_CONTINUE, 
		error, 
		ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider(), 
		L"iisnode posts completion from ChildContextCompleted", 
		WINEVENT_LEVEL_VERBOSE);
	
	return;
}

void WINAPI CProtocolBridge::CreateNamedPipeConnection(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
	HRESULT hr;
	CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);
	HANDLE pipe;

	ErrorIf(INVALID_HANDLE_VALUE == (pipe = CreateFile(
		ctx->GetNodeProcess()->GetNamedPipeName(),
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_OVERLAPPED,
		NULL)), 
		GetLastError());

	ctx->SetPipe(pipe);
	ctx->GetNodeApplication()->GetApplicationManager()->GetAsyncManager()->AddAsyncCompletionHandle(pipe);

	ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode created named pipe connection to the node.exe process", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
	
	CProtocolBridge::SendHttpRequestHeaders(ctx);

	return;

Error:

	DWORD retry = ctx->GetConnectionRetryCount();
	if (retry >= CModuleConfiguration::GetMaxNamedPipeConnectionRetry(ctx->GetHttpContext()))
	{
		if (hr == ERROR_PIPE_BUSY)
		{
			ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
				L"iisnode was unable to establish named pipe connection to the node.exe process because the named pipe server is too busy", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
			CProtocolBridge::SendEmptyResponse(ctx, 503, _T("Service Unavailable"), hr);
		}
		else
		{
			ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
				L"iisnode was unable to establish named pipe connection to the node.exe process", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
			CProtocolBridge::SendEmptyResponse(ctx, 500, _T("Internal Server Error"), hr);
		}
	}
	else 
	{
		ctx->SetConnectionRetryCount(retry + 1);
		ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode scheduled a retry of a named pipe connection to the node.exe process ", WINEVENT_LEVEL_INFO, ctx->GetActivityId());
		CProtocolBridge::PostponeProcessing(ctx, CModuleConfiguration::GetNamedPipeConnectionRetryDelay(ctx->GetHttpContext()));
	}

	return;
}

void CProtocolBridge::SendHttpRequestHeaders(CNodeHttpStoredContext* context)
{
	HRESULT hr;
	DWORD length;

	// capture ETW provider since after a successful call to WriteFile the context may be asynchronously deleted

	CNodeEventProvider* etw = context->GetNodeApplication()->GetApplicationManager()->GetEventProvider();
	GUID activityId;
	memcpy(&activityId, context->GetActivityId(), sizeof GUID);

	// request the named pipe to be closed by the server after the response is sent
	// since we are not reusing named pipe connections anyway, requesting the server 
	// to close it after sending the response allows the module to avoid parsing chunked responses
	// to detect end of response

	CheckError(context->GetHttpContext()->GetRequest()->SetHeader("Connection", "close", 5, TRUE));

	CheckError(CHttpProtocol::SerializeRequestHeaders(context, context->GetBufferRef(), context->GetBufferSizeRef(), &length));

	context->SetNextProcessor(CProtocolBridge::SendHttpRequestHeadersCompleted);
	
	if (WriteFile(context->GetPipe(), context->GetBuffer(), length, NULL, context->InitializeOverlapped()))
	{
		// completed synchronously

		etw->Log(L"iisnode initiated sending http request headers to the node.exe process and completed synchronously", 
			WINEVENT_LEVEL_VERBOSE, 
			&activityId);

		// asynchronous callback will be invoked and processing will continue asynchronously since IO completion ports are used
		// - see http://msdn.microsoft.com/en-us/library/windows/desktop/aa365683(v=vs.85).aspx
	}
	else 
	{
		hr = GetLastError();
		if (ERROR_IO_PENDING == hr)
		{
			// will complete asynchronously

			etw->Log(L"iisnode initiated sending http request headers to the node.exe process and will complete asynchronously", 
				WINEVENT_LEVEL_VERBOSE, 
				&activityId);
		}
		else 
		{
			// error

			etw->Log(L"iisnode failed to initiate sending http request headers to the node.exe process", 
				WINEVENT_LEVEL_ERROR, 
				&activityId);

			CProtocolBridge::SendEmptyResponse(context, 500, _T("Internal Server Error"), hr);		
		}
	}

	return;

Error:

	etw->Log(L"iisnode failed to serialize http request headers", 
		WINEVENT_LEVEL_ERROR, 
		&activityId);

	CProtocolBridge::SendEmptyResponse(context, 500, _T("Internal Server Error"), hr);		

	return;
}

void WINAPI CProtocolBridge::SendHttpRequestHeadersCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
	HRESULT hr;
	CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

	CheckError(error);
	ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode finished sending http request headers to the node.exe process", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
	CProtocolBridge::ReadRequestBody(ctx);

	return;
Error:

	ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode failed to send http request headers to the node.exe process", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
	CProtocolBridge::SendEmptyResponse(ctx, 500, _T("Internal Server Error"), hr);

	return;
}

void CProtocolBridge::ReadRequestBody(CNodeHttpStoredContext* context)
{
	HRESULT hr;	
	DWORD bytesReceived;
	BOOL completionPending = FALSE;

	context->SetNextProcessor(CProtocolBridge::ReadRequestBodyCompleted);
	CheckError(context->GetHttpContext()->GetRequest()->ReadEntityBody(context->GetBuffer(), context->GetBufferSize(), TRUE, &bytesReceived, &completionPending));

	if (!completionPending)
	{
		context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode initiated reading http request body chunk and completed synchronously", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());

		CProtocolBridge::ReadRequestBodyCompleted(S_OK, bytesReceived, context->GetOverlapped());
	}
	else
	{
		context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode initiated reading http request body chunk and will complete asynchronously", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());
	}

	return;
Error:

	if (HRESULT_FROM_WIN32(ERROR_HANDLE_EOF) == hr)
	{
		context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode detected the end of the http request body", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());
		CProtocolBridge::StartReadResponse(context);
	}
	else
	{
		context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode failed reading http request body", WINEVENT_LEVEL_ERROR, context->GetActivityId());
		CProtocolBridge::SendEmptyResponse(context, 500, _T("Internal Server Error"), HRESULT_FROM_WIN32(hr));
	}

	return;
}

void WINAPI CProtocolBridge::ReadRequestBodyCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{	
	CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

	if (S_OK == error && bytesTransfered > 0)
	{
		ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode read a chunk of http request body", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
		CProtocolBridge::SendRequestBody(ctx, bytesTransfered);
	}
	else if (ERROR_HANDLE_EOF == error || 0 == bytesTransfered)
	{	
		ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode detected the end of the http request body", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
		CProtocolBridge::StartReadResponse(ctx);
	}
	else 
	{
		ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode failed reading http request body", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
		CProtocolBridge::SendEmptyResponse(ctx, 500, _T("Internal Server Error"), error);
	}
}

void CProtocolBridge::SendRequestBody(CNodeHttpStoredContext* context, DWORD chunkLength)
{
	// capture ETW provider since after a successful call to WriteFile the context may be asynchronously deleted

	CNodeEventProvider* etw = context->GetNodeApplication()->GetApplicationManager()->GetEventProvider();
	GUID activityId;
	memcpy(&activityId, context->GetActivityId(), sizeof GUID);

	context->SetNextProcessor(CProtocolBridge::SendRequestBodyCompleted);

	if (WriteFile(context->GetPipe(), context->GetBuffer(), chunkLength, NULL, context->InitializeOverlapped()))
	{
		// completed synchronously

		etw->Log(L"iisnode initiated sending http request body chunk to the node.exe process and completed synchronously", 
			WINEVENT_LEVEL_VERBOSE, 
			&activityId);

		// asynchronous callback will be invoked and processing will continue asynchronously since IO completion ports are used
		// - see http://msdn.microsoft.com/en-us/library/windows/desktop/aa365683(v=vs.85).aspx
	}
	else 
	{
		HRESULT hr = GetLastError();

		if (ERROR_IO_PENDING == hr)
		{
			// will complete asynchronously

			etw->Log(L"iisnode initiated sending http request body chunk to the node.exe process and will complete asynchronously", 
				WINEVENT_LEVEL_VERBOSE, 
				&activityId);
		}
		else if (ERROR_NO_DATA == hr)
		{
			// Node.exe has closed the named pipe. This means it does not expect any more request data, but it does not mean there is no response.
			// This may happen even for POST requests if the node.js application does not register event handlers for the 'data' or 'end' request events.
			// Ignore the write error and attempt to read the response instead (which might have been written by node.exe before the named pipe connection 
			// was closed). 

			etw->Log(L"iisnode detected the node.exe process closed the named pipe connection", 
				WINEVENT_LEVEL_VERBOSE, 
				&activityId);

			CProtocolBridge::StartReadResponse(context);
		}
		else
		{
			// error

			etw->Log(L"iisnode failed to initiate sending http request body chunk to the node.exe process", 
				WINEVENT_LEVEL_ERROR, 
				&activityId);

			CProtocolBridge::SendEmptyResponse(context, 500, _T("Internal Server Error"), hr);
		}
	}

	return;
}

void WINAPI CProtocolBridge::SendRequestBodyCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
	CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

	if (S_OK == error)
	{
		ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode finished sending http request body chunk to the node.exe process", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
		CProtocolBridge::ReadRequestBody(ctx);
	}
	else 
	{
		ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode failed to send http request body chunk to the node.exe process", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
		CProtocolBridge::SendEmptyResponse(ctx, 500, _T("Internal Server Error"), error);
	}
}

void CProtocolBridge::StartReadResponse(CNodeHttpStoredContext* context)
{
	context->SetDataSize(0);
	context->SetParsingOffset(0);
	context->SetNextProcessor(CProtocolBridge::ProcessResponseStatusLine);
	context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode starting to read http response", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());
	CProtocolBridge::ContinueReadResponse(context);
}

HRESULT CProtocolBridge::EnsureBuffer(CNodeHttpStoredContext* context)
{
	HRESULT hr;

	if (context->GetBufferSize() == context->GetDataSize())
	{
		// buffer is full

		if (context->GetParsingOffset() > 0)
		{
			// remove already parsed data from buffer by moving unprocessed data to the front; this will free up space at the end

			char* b = (char*)context->GetBuffer();
			memcpy(b, b + context->GetParsingOffset(), context->GetDataSize() - context->GetParsingOffset());
		}
		else 
		{
			// allocate more buffer memory

			context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
				L"iisnode allocating more buffer memory to handle http response", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());

			DWORD* bufferLength = context->GetBufferSizeRef();
			void** buffer = context->GetBufferRef();

			DWORD quota = CModuleConfiguration::GetMaxRequestBufferSize(context->GetHttpContext());
			ErrorIf(*bufferLength >= quota, ERROR_NOT_ENOUGH_QUOTA);

			void* newBuffer;
			DWORD newBufferLength = *bufferLength * 2;
			if (newBufferLength > quota)
			{
				newBufferLength = quota;
			}

			ErrorIf(NULL == (newBuffer = context->GetHttpContext()->AllocateRequestMemory(newBufferLength)), ERROR_NOT_ENOUGH_MEMORY);
			memcpy(newBuffer, (char*)(*buffer) + context->GetParsingOffset(), context->GetDataSize() - context->GetParsingOffset());
			*buffer = newBuffer;
			*bufferLength = newBufferLength;
		}

		context->SetDataSize(context->GetDataSize() - context->GetParsingOffset());
		context->SetParsingOffset(0);
	}

	return S_OK;
Error:
	return hr;

}

void CProtocolBridge::ContinueReadResponse(CNodeHttpStoredContext* context)
{
	HRESULT hr;

	// capture ETW provider since after a successful call to ReadFile the context may be asynchronously deleted

	CNodeEventProvider* etw = context->GetNodeApplication()->GetApplicationManager()->GetEventProvider();
	GUID activityId;
	memcpy(&activityId, context->GetActivityId(), sizeof GUID);

	CheckError(CProtocolBridge::EnsureBuffer(context));

	if (ReadFile(
			context->GetPipe(), 
			(char*)context->GetBuffer() + context->GetDataSize(), 
			context->GetBufferSize() - context->GetDataSize(),
			NULL,
			context->InitializeOverlapped()))
	{
		// read completed synchronously 

		etw->Log(L"iisnode initiated reading http response chunk and completed synchronously", 
			WINEVENT_LEVEL_VERBOSE, 
			&activityId);

		// asynchronous callback will be invoked and processing will continue asynchronously since IO completion ports are used
		// - see http://msdn.microsoft.com/en-us/library/windows/desktop/aa365683(v=vs.85).aspx
	}
	else if (ERROR_IO_PENDING == (hr = GetLastError()))
	{
		// read will complete asynchronously

		etw->Log(L"iisnode initiated reading http response chunk and will complete asynchronously", 
			WINEVENT_LEVEL_VERBOSE, 
			&activityId);
	}
	else if (context->GetResponseContentLength() == -1)
	{
		// connection termination with chunked transfer encoding indicates end of response
		// since we have sent Connection: close HTTP request header to node from SendHttpRequestHeaders

		etw->Log(L"iisnode iniatiated reading http response chunk and synchronously detected the end of the http response", 
			WINEVENT_LEVEL_VERBOSE, 
			&activityId);

		// CR: narrow down this condition to orderly pipe closure
		CProtocolBridge::FinalizeResponse(context);
	}
	else
	{
		// error

		etw->Log(L"iisnode failed to initialize reading of http response chunk", 
			WINEVENT_LEVEL_ERROR, 
			&activityId);

		CProtocolBridge::SendEmptyResponse(context, 500, _T("Internal Server Error"), hr);
	}

	return;
Error:

	context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode failed to allocate memory buffer to read http response chunk", WINEVENT_LEVEL_ERROR, &activityId);

	CProtocolBridge::SendEmptyResponse(context, 500, _T("Internal Server Error"), hr);

	return;
}

void WINAPI CProtocolBridge::ProcessResponseStatusLine(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
	HRESULT hr;
	CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

	ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode starting to process http response status line", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

	CheckError(error);
	ctx->SetDataSize(ctx->GetDataSize() + bytesTransfered);
	CheckError(CHttpProtocol::ParseResponseStatusLine(ctx));

	ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode finished processing http response status line", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

	ctx->SetNextProcessor(CProtocolBridge::ProcessResponseHeaders);
	CProtocolBridge::ProcessResponseHeaders(S_OK, 0, ctx->GetOverlapped());

	return;
Error:

	if (ERROR_MORE_DATA == hr)
	{
		CProtocolBridge::ContinueReadResponse(ctx);
	}
	else
	{
		ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode failed to process http response status line", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
		CProtocolBridge::SendEmptyResponse(ctx, 500, _T("Internal Server Error"), hr);
	}

	return;
}

void WINAPI CProtocolBridge::ProcessResponseHeaders(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
	HRESULT hr;
	CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);
	PCSTR contentLength;
	USHORT contentLengthLength;

	ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode starting to process http response headers", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

	CheckError(error);
	ctx->SetDataSize(ctx->GetDataSize() + bytesTransfered);
	CheckError(CHttpProtocol::ParseResponseHeaders(ctx));

	contentLength = ctx->GetHttpContext()->GetResponse()->GetHeader(HttpHeaderContentLength, &contentLengthLength);
	if (0 == contentLengthLength)
	{
		ctx->SetResponseContentLength(-1);
	}
	else
	{
		LONGLONG length = 0;
		int i = 0;

		// skip leading white space
		while (i < contentLengthLength && (contentLength[i] < '0' || contentLength[i] > '9')) 
			i++;

		while (i < contentLengthLength && contentLength[i] >= '0' && contentLength[i] <= '9') 
			length = length * 10 + contentLength[i++] - '0';

		ctx->SetResponseContentLength(length);
	}

	ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode finished processing http response headers", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

	ctx->SetNextProcessor(CProtocolBridge::ProcessResponseBody);
	CProtocolBridge::ProcessResponseBody(S_OK, 0, ctx->GetOverlapped());

	return;
Error:

	if (ERROR_MORE_DATA == hr)
	{
		CProtocolBridge::ContinueReadResponse(ctx);
	}
	else
	{
		ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode failed to process http response headers", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
		CProtocolBridge::SendEmptyResponse(ctx, 500, _T("Internal Server Error"), hr);
	}

	return;
}

void WINAPI CProtocolBridge::ProcessResponseBody(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
	HRESULT hr;
	CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);
	HTTP_DATA_CHUNK* chunk;
	DWORD bytesSent;
	BOOL completionExpected;

	ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode starting to process http response body", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

	if (S_OK != error)
	{
		if (ctx->GetResponseContentLength() == -1)
		{
			// connection termination with chunked transfer encoding indicates end of response
			// since we have sent Connection: close HTTP request header to node from SendHttpRequestHeaders

			ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
				L"iisnode detected the end of the http response", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

			// CR: check the other commend for finalizing response
			CProtocolBridge::FinalizeResponse(ctx);
		}
		else
		{
			ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
				L"iisnode failed to read http response body", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
			CProtocolBridge::SendEmptyResponse(ctx, 500, _T("Internal Server Error"), error);
		}

		return;
	}

	ctx->SetDataSize(ctx->GetDataSize() + bytesTransfered);

	if (ctx->GetDataSize() > ctx->GetParsingOffset())
	{
		// send body data to client

		// CR: consider using malloc here (memory can be released after Flush)

		ErrorIf(NULL == (chunk = (HTTP_DATA_CHUNK*) ctx->GetHttpContext()->AllocateRequestMemory(sizeof HTTP_DATA_CHUNK)), ERROR_NOT_ENOUGH_MEMORY);
		chunk->DataChunkType = HttpDataChunkFromMemory;
		chunk->FromMemory.BufferLength = ctx->GetDataSize() - ctx->GetParsingOffset();
		ErrorIf(NULL == (chunk->FromMemory.pBuffer = ctx->GetHttpContext()->AllocateRequestMemory(chunk->FromMemory.BufferLength)), ERROR_NOT_ENOUGH_MEMORY);
		memcpy(chunk->FromMemory.pBuffer, (char*)ctx->GetBuffer() + ctx->GetParsingOffset(), chunk->FromMemory.BufferLength);

		ctx->SetDataSize(0);
		ctx->SetParsingOffset(0);
		ctx->SetNextProcessor(CProtocolBridge::SendResponseBodyCompleted);

		CheckError(ctx->GetHttpContext()->GetResponse()->WriteEntityChunks(
			chunk,
			1,
			TRUE,
			ctx->GetResponseContentLength() == -1 || ctx->GetResponseContentLength() > (ctx->GetResponseContentTransmitted() + chunk->FromMemory.BufferLength),
			&bytesSent,
			&completionExpected));

		ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode started sending http response body chunk", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
		
		if (!completionExpected)
		{
			CProtocolBridge::SendResponseBodyCompleted(S_OK, chunk->FromMemory.BufferLength, ctx->GetOverlapped());
		}
	}
	else if (-1 == ctx->GetResponseContentLength() || ctx->GetResponseContentLength() > ctx->GetResponseContentTransmitted())
	{
		// read more body data

		CProtocolBridge::ContinueReadResponse(ctx);
	}
	else
	{
		// finish request

		CProtocolBridge::FinalizeResponse(ctx);
	}

	return;
Error:

	ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode failed to send http response body chunk", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
	CProtocolBridge::SendEmptyResponse(ctx, 500, _T("Internal Server Error"), hr);

	return;
}

void WINAPI CProtocolBridge::SendResponseBodyCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
	HRESULT hr;
	CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);
	DWORD bytesSent;
	BOOL completionExpected = FALSE;

	CheckError(error);
	ctx->SetResponseContentTransmitted(ctx->GetResponseContentTransmitted() + bytesTransfered);

	if (ctx->GetResponseContentLength() == -1 || ctx->GetResponseContentLength() > ctx->GetResponseContentTransmitted())
	{
		if (ctx->GetResponseContentLength() == -1)
		{
			// Flush chunked responses. 
			// TODO, tjanczuk, is flushing chunked responses requried

			ctx->SetNextProcessor(CProtocolBridge::ContinueProcessResponseBodyAfterPartialFlush);
			ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
				L"iisnode initiated flushing http response body chunk", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
			ctx->GetHttpContext()->GetResponse()->Flush(TRUE, TRUE, &bytesSent, &completionExpected);
		}

		if (!completionExpected)
		{
			CProtocolBridge::ContinueProcessResponseBodyAfterPartialFlush(S_OK, 0, ctx->GetOverlapped());
		}
	}
	else
	{
		CProtocolBridge::FinalizeResponse(ctx);
	}

	return;
Error:

	ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode failed to flush http response body chunk", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
	CProtocolBridge::SendEmptyResponse(ctx, 500, _T("Internal Server Error"), hr);

	return;
}

void WINAPI CProtocolBridge::ContinueProcessResponseBodyAfterPartialFlush(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
	HRESULT hr;
	CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

	CheckError(error);	
	ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode finished flushing http response body chunk", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
	ctx->SetNextProcessor(CProtocolBridge::ProcessResponseBody);
	CProtocolBridge::ContinueReadResponse(ctx);

	return;
Error:

	ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode failed to flush http response body chunk", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
	CProtocolBridge::SendEmptyResponse(ctx, 500, _T("Internal Server Error"), hr);
	
	return;
}

void CProtocolBridge::FinalizeResponse(CNodeHttpStoredContext* context)
{
	context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode finished processing http request/response", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());

	CloseHandle(context->GetPipe());
	context->SetPipe(INVALID_HANDLE_VALUE);
	CProtocolBridge::FinalizeResponseCore(
		context, 
		RQ_NOTIFICATION_CONTINUE, 
		S_OK, 
		context->GetNodeApplication()->GetApplicationManager()->GetEventProvider(),
		L"iisnode posts completion from FinalizeResponse", 
		WINEVENT_LEVEL_VERBOSE);
}

HRESULT CProtocolBridge::FinalizeResponseCore(CNodeHttpStoredContext* context, REQUEST_NOTIFICATION_STATUS status, HRESULT error, CNodeEventProvider* log, PCWSTR etw, UCHAR level)
{
	context->SetRequestNotificationStatus(status);
	context->SetNextProcessor(NULL);
	context->SetHresult(error);

	if (NULL != context->GetNodeProcess())
	{
		// there is no CNodeProcess assigned to the request yet - something failed before it was moved from the pending queue
		// to an active request queue of a specific process

		context->GetNodeProcess()->OnRequestCompleted(context);
	}

	if (0 == context->DecreasePendingAsyncOperationCount()) // decreases ref count increased in the ctor of CPendingRequestQueue::Push
	{
		log->Log(etw, level, context->GetActivityId());		
		context->GetHttpContext()->PostCompletion(0);
	}

	return S_OK;
}

HRESULT CProtocolBridge::SendDebugRedirect(CNodeHttpStoredContext* context, CNodeEventProvider* log)
{
	HRESULT hr;

	IHttpContext* ctx = context->GetHttpContext();
	IHttpResponse* response = ctx->GetResponse();
	HTTP_REQUEST* raw = ctx->GetRequest()->GetRawHttpRequest();

	// redirect app.js/debug to app.js/debug/ by appending '/' at the end of the path

	PSTR path;
	int pathSizeA;
	ErrorIf(0 == (pathSizeA = WideCharToMultiByte(CP_ACP, 0, raw->CookedUrl.pAbsPath, raw->CookedUrl.AbsPathLength >> 1, NULL, 0, NULL, NULL)), E_FAIL);
	ErrorIf(NULL == (path = (TCHAR*)ctx->AllocateRequestMemory(pathSizeA + 2)), ERROR_NOT_ENOUGH_MEMORY);
	ErrorIf(pathSizeA != WideCharToMultiByte(CP_ACP, 0, raw->CookedUrl.pAbsPath, raw->CookedUrl.AbsPathLength >> 1, path, pathSizeA, NULL, NULL), E_FAIL);
	path[pathSizeA] = '/';
	path[pathSizeA + 1] = 0;

	response->SetStatus(301, "Moved Permanently"); 
	CheckError(context->GetHttpContext()->GetResponse()->Redirect(path, FALSE, TRUE));
	CProtocolBridge::FinalizeResponseCore(
		context, 
		RQ_NOTIFICATION_FINISH_REQUEST, 
		S_OK, 
		log,
		L"iisnode redirected debugging request", 
		WINEVENT_LEVEL_VERBOSE);

	return S_OK;
Error:
	return hr;
}