#include "precomp.h"

HRESULT CProtocolBridge::PostponeProcessing(CNodeHttpStoredContext* context, DWORD dueTime)
{
	CAsyncManager* async = context->GetNodeApplication()->GetApplicationManager()->GetAsyncManager();
	LARGE_INTEGER delay;
	delay.QuadPart = dueTime;

	return async->SetTimer(context->GetAsyncContext(), &delay);
}

HRESULT CProtocolBridge::SendEmptyResponse(CNodeHttpStoredContext* context, USHORT status, PCTSTR reason, HRESULT hresult)
{
	// TODO, tjanczuk, return an empty response and terminate request processing
	return S_OK;
}

HRESULT CProtocolBridge::InitiateRequest(CNodeHttpStoredContext* context)
{
	context->SetNextProcessor(CProtocolBridge::CreateNamedPipeConnection);
	CProtocolBridge::CreateNamedPipeConnection(S_OK, 0, context->GetOverlapped());

	return S_OK;
}

void WINAPI CProtocolBridge::CreateNamedPipeConnection(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
	HRESULT hr;
	CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);
	HANDLE pipe;

	ErrorIf(INVALID_HANDLE_VALUE == (pipe = CreateFile(
		ctx->GetNodeProcess()->GetNamedPipeName(),
		GENERIC_READ | GENERIC_WRITE | FILE_FLAG_OVERLAPPED,
		0,
		NULL,
		OPEN_EXISTING,
		0,
		NULL)), 
		GetLastError());

	ctx->SetPipe(pipe);
	ctx->GetNodeApplication()->GetApplicationManager()->GetAsyncManager()->AddAsyncCompletionHandle(pipe);
	
	CProtocolBridge::SendHttpRequestHeaders(ctx);

	return;

Error:

	if (hr == ERROR_PIPE_BUSY)
	{
		DWORD retry = ctx->GetConnectionRetryCount();
		if (retry >= CModuleConfiguration::GetMaxNamedPipeConnectionRetry())
		{
			CProtocolBridge::SendEmptyResponse(ctx, 503, "Service Unavailable", hr);
		}
		else 
		{
			ctx->SetConnectionRetryCount(retry + 1);
			CProtocolBridge::PostponeProcessing(ctx, CModuleConfiguration::GetNamePipeConnectionRetryDelay());
		}
	}
	else 
	{
		CProtocolBridge::SendEmptyResponse(ctx, 500, _T("Internal Server Error"), hr);
	}

	return;
}

void CProtocolBridge::SendHttpRequestHeaders(CNodeHttpStoredContext* context)
{
	HRESULT hr;
	DWORD length;

	CheckError(CHttpProtocol::SerializeRequestHeaders(context->GetHttpContext(), context->GetBufferRef(), context->GetBufferSizeRef(), &length));

	context->SetNextProcessor(CProtocolBridge::SendHttpRequestHeadersCompleted);
	hr = WriteFile(context->GetPipe(), context->GetBuffer(), length, NULL, context->GetOverlapped()) ? S_OK : GetLastError();
	ErrorIf(hr != S_OK && hr != ERROR_IO_PENDING, hr);

	return;

Error:

	CProtocolBridge::SendEmptyResponse(context, 500, _T("Internal Server Error"), hr);

	return;
}

void WINAPI CProtocolBridge::SendHttpRequestHeadersCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
	CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

	if (S_OK == error)
	{
		CProtocolBridge::ReadRequestBody(ctx);
	}
	else
	{
		CProtocolBridge::SendEmptyResponse(ctx, 500, _T("Internal Server Error"), error);
	}
}

void CProtocolBridge::ReadRequestBody(CNodeHttpStoredContext* context)
{
	HRESULT hr;	
	DWORD bytesReceived;
	BOOL completionPending;

	context->SetNextProcessor(CProtocolBridge::ReadRequestBodyCompleted);
	CheckError(context->GetHttpContext()->GetRequest()->ReadEntityBody(context->GetBuffer(), context->GetBufferSize(), TRUE, &bytesReceived, &completionPending));
	if (!completionPending)
	{
		CProtocolBridge::ReadRequestBodyCompleted(S_OK, bytesReceived, context->GetOverlapped());
	}

	return;
Error:

	if (ERROR_HANDLE_EOF == hr)
	{
		CProtocolBridge::ReadResponseHeaders(context);
	}
	else
	{
		CProtocolBridge::SendEmptyResponse(context, 500, _T("Internal Server Error"), hr);
	}

	return;
}

void WINAPI CProtocolBridge::ReadRequestBodyCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{	
	CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

	if (S_OK == error && bytesTransfered > 0)
	{
		CProtocolBridge::SendRequestBody(ctx, bytesTransfered);
	}
	else if (ERROR_HANDLE_EOF == error)
	{
		CProtocolBridge::ReadResponseHeaders(ctx);
	}
	else 
	{
		CProtocolBridge::SendEmptyResponse(ctx, 500, _T("Internal Server Error"), error);
	}
}

void CProtocolBridge::SendRequestBody(CNodeHttpStoredContext* context, DWORD chunkLength)
{
	HRESULT hr;

	context->SetNextProcessor(CProtocolBridge::SendRequestBodyCompleted);
	hr = WriteFile(context->GetPipe(), context->GetBuffer(), chunkLength, NULL, context->GetOverlapped()) ? S_OK : GetLastError();
	ErrorIf(hr != S_OK && hr != ERROR_IO_PENDING, hr);

	return;

Error:

	CProtocolBridge::SendEmptyResponse(context, 500, _T("Internal Server Error"), hr);

	return;
}

void WINAPI CProtocolBridge::SendRequestBodyCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
	CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

	if (S_OK == error)
	{
		CProtocolBridge::ReadRequestBody(ctx);
	}
	else 
	{
		CProtocolBridge::SendEmptyResponse(ctx, 500, _T("Internal Server Error"), error);
	}
}

void CProtocolBridge::ReadResponseHeaders(CNodeHttpStoredContext* context)
{
}
