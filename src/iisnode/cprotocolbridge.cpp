#include "precomp.h"

HRESULT CProtocolBridge::PostponeProcessing(CNodeHttpStoredContext* context, DWORD dueTime)
{
	CAsyncManager* async = context->GetNodeApplication()->GetApplicationManager()->GetAsyncManager();
	LARGE_INTEGER delay;
	delay.QuadPart = dueTime;
	delay.QuadPart *= -10000; // convert from ms to 100ns units

	return async->SetTimer(context->GetAsyncContext(), &delay);
}

HRESULT CProtocolBridge::SendEmptyResponse(CNodeHttpStoredContext* context, USHORT status, PCTSTR reason, HRESULT hresult)
{
	context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode request processing failed", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());

	if (INVALID_HANDLE_VALUE != context->GetPipe())
	{
		CloseHandle(context->GetPipe());
		context->SetPipe(INVALID_HANDLE_VALUE);
	}

	context->SetHresult(hresult);
	context->SetRequestNotificationStatus(RQ_NOTIFICATION_FINISH_REQUEST);	
	context->SetNextProcessor(NULL);
	
	if (NULL != context->GetNodeProcess())
	{
		// there is no CNodeProcess assigned to the request yet - something failed before it was moved from the pending queue
		// to an active request queue of a specific process

		context->GetNodeProcess()->OnRequestCompleted(context);
	}

	CProtocolBridge::SendEmptyResponse(context->GetHttpContext(), status, reason, hresult);	

	if (0 == context->DecreasePendingAsyncOperationCount()) // decreases ref count increased in CPendingRequestQueue::Push
	{		
		context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode posts completion from SendEmtpyResponse", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());		
		return context->GetHttpContext()->PostCompletion(0);
	}

	return S_OK;
}

void CProtocolBridge::SendEmptyResponse(IHttpContext* httpCtx, USHORT status, PCTSTR reason, HRESULT hresult)
{
	if (!httpCtx->GetResponseHeadersSent())
	{
		httpCtx->GetResponse()->Clear();
		httpCtx->GetResponse()->SetStatus(status, reason, 0, hresult);
	}
}

HRESULT CProtocolBridge::InitiateRequest(CNodeHttpStoredContext* context)
{
	context->SetNextProcessor(CProtocolBridge::CreateNamedPipeConnection);
	CProtocolBridge::CreateNamedPipeConnection(S_OK, 0, context->InitializeOverlapped());

	return context->GetHresult(); // synchronous completion HRESULT
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
		FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH | FILE_FLAG_OVERLAPPED,
		NULL)), 
		GetLastError());

	DWORD mode = PIPE_READMODE_BYTE;// | PIPE_NOWAIT;
	ErrorIf(!SetNamedPipeHandleState(
		pipe, 
		&mode,
		NULL,
		NULL), 
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

	// request the named pipe to be closed by the server after the response is sent
	// since we are not reusing named pipe connections anyway, requesting the server 
	// to close it after sending the response allows the module to avoid parsing chunked responses
	// to detect end of response

	CheckError(context->GetHttpContext()->GetRequest()->SetHeader("Connection", "close", 5, TRUE));

	CheckError(CHttpProtocol::SerializeRequestHeaders(context->GetHttpContext(), context->GetBufferRef(), context->GetBufferSizeRef(), &length));

	context->SetNextProcessor(CProtocolBridge::SendHttpRequestHeadersCompleted);
	
	if (WriteFile(context->GetPipe(), context->GetBuffer(), length, NULL, context->InitializeOverlapped()))
	{
		// completed synchronously

		etw->Log(L"iisnode initiated sending http request headers to the node.exe process and completed synchronously", 
			WINEVENT_LEVEL_VERBOSE, 
			context->GetActivityId());

		// asynchronous callback will be invoked and processing will continue asynchronously since IO completion ports are used
		// - see http://msdn.microsoft.com/en-us/library/windows/desktop/aa365683(v=vs.85).aspx
	}
	else if (ERROR_IO_PENDING == GetLastError())
	{
		// will complete asynchronously

		etw->Log(L"iisnode initiated sending http request headers to the node.exe process and will complete asynchronously", 
			WINEVENT_LEVEL_VERBOSE, 
			context->GetActivityId());
	}
	else 
	{
		// error

		etw->Log(L"iisnode failed initiate sending http request headers to the node.exe process", 
			WINEVENT_LEVEL_ERROR, 
			context->GetActivityId());

		CProtocolBridge::SendEmptyResponse(context, 500, _T("Internal Server Error"), hr);		
	}

	return;

Error:

	etw->Log(L"iisnode failed to serialize http request headers", 
		WINEVENT_LEVEL_ERROR, 
		context->GetActivityId());

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
			L"iisnodeinitiated reading http request body chunk and completed synchronously", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());

		CProtocolBridge::ReadRequestBodyCompleted(S_OK, bytesReceived, context->GetOverlapped());
	}
	else
	{
		context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnodeinitiated reading http request body chunk and will complete asynchronously", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());
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
			L"iisnoderead a chunk of http request body", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
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

	context->SetNextProcessor(CProtocolBridge::SendRequestBodyCompleted);

	if (WriteFile(context->GetPipe(), context->GetBuffer(), chunkLength, NULL, context->InitializeOverlapped()))
	{
		// completed synchronously

		etw->Log(L"iisnode initiated sending http request body chunk to the node.exe process and completed synchronously", 
			WINEVENT_LEVEL_VERBOSE, 
			context->GetActivityId());

		// asynchronous callback will be invoked and processing will continue asynchronously since IO completion ports are used
		// - see http://msdn.microsoft.com/en-us/library/windows/desktop/aa365683(v=vs.85).aspx
	}
	else if (ERROR_IO_PENDING == GetLastError())
	{
		// will complete asynchronously

		etw->Log(L"iisnode initiated sending http request body chunk to the node.exe process and will complete asynchronously", 
			WINEVENT_LEVEL_VERBOSE, 
			context->GetActivityId());
	}
	else 
	{
		// error

		HRESULT hr = GetLastError();
		
		etw->Log(L"iisnode failed to initiate sending http request body chunk to the node.exe process", 
			WINEVENT_LEVEL_ERROR, 
			context->GetActivityId());

		CProtocolBridge::SendEmptyResponse(context, 500, _T("Internal Server Error"), hr);
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
			context->GetActivityId());

		// asynchronous callback will be invoked and processing will continue asynchronously since IO completion ports are used
		// - see http://msdn.microsoft.com/en-us/library/windows/desktop/aa365683(v=vs.85).aspx
	}
	else if (ERROR_IO_PENDING == GetLastError())
	{
		// read will complete asynchronously

		etw->Log(L"iisnode initiated reading http response chunk and will complete asynchronously", 
			WINEVENT_LEVEL_VERBOSE, 
			context->GetActivityId());
	}
	else if (context->GetResponseContentLength() == -1)
	{
		// connection termination with chunked transfer encoding indicates end of response
		// since we have sent Connection: close HTTP request header to node from SendHttpRequestHeaders

		etw->Log(L"iisnode iniatiated reading http response chunk and synchronously detected the end of the http response", 
			WINEVENT_LEVEL_VERBOSE, 
			context->GetActivityId());

		// CR: narrow down this condition to orderly pipe closure
		CProtocolBridge::FinalizeResponse(context);
	}
	else
	{
		// error

		etw->Log(L"iisnode failed to initialize reading of http response chunk", 
			WINEVENT_LEVEL_ERROR, 
			context->GetActivityId());

		CProtocolBridge::SendEmptyResponse(context, 500, _T("Internal Server Error"), hr);
	}

	return;
Error:

	context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
		L"iisnode failed to allocate memory buffer to read http response chunk", WINEVENT_LEVEL_ERROR, context->GetActivityId());

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
	context->GetNodeProcess()->OnRequestCompleted(context);
	context->SetRequestNotificationStatus(RQ_NOTIFICATION_CONTINUE);
	context->SetNextProcessor(NULL);

	if (0 == context->DecreasePendingAsyncOperationCount()) // decreases ref count increased in the ctor of CPendingRequestQueue::Push
	{
		context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode posts completion from FinalizeResponse", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());		
		context->GetHttpContext()->PostCompletion(0);
	}
}