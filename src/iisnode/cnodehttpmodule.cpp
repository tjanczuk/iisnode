#include "precomp.h"

CNodeHttpModule::CNodeHttpModule(CNodeApplicationManager* applicationManager)
	: applicationManager(applicationManager)
{
}

REQUEST_NOTIFICATION_STATUS CNodeHttpModule::OnExecuteRequestHandler(
	IN IHttpContext* pHttpContext, 
	IN IHttpEventProvider* pProvider)
{
	HRESULT hr;
	CNodeHttpStoredContext* ctx = NULL;

	CheckError(this->applicationManager->Initialize(pHttpContext));

	this->applicationManager->GetEventProvider()->Log(L"iisnode received a new http request", WINEVENT_LEVEL_INFO);

	// reject websocket connections since iisnode does not support them
	// http://tools.ietf.org/html/draft-ietf-hybi-thewebsocketprotocol-17#page-17

	PCSTR upgrade = pHttpContext->GetRequest()->GetHeader(HttpHeaderUpgrade, NULL);
	ErrorIf(upgrade && 0 == strcmp("websocket", upgrade), ERROR_NOT_SUPPORTED);		

	CheckError(this->applicationManager->Dispatch(pHttpContext, pProvider, &ctx));

	if (0 == ctx->DecreasePendingAsyncOperationCount()) // decreases ref count set to 1 in the ctor of CNodeHttpStoredContext
	{
		return ctx->GetRequestNotificationStatus();
	}
	else
	{
		return RQ_NOTIFICATION_PENDING;
	}

Error:

	CNodeEventProvider* log = this->applicationManager->GetEventProvider();

	if (log)
	{
		log->Log(L"iisnode failed to process a new http request", WINEVENT_LEVEL_INFO);
	}

	if (ERROR_NOT_ENOUGH_QUOTA == hr)
	{
		CProtocolBridge::SendEmptyResponse(pHttpContext, 503, _T("Service Unavailable"), hr);
	}
	else if (ERROR_FILE_NOT_FOUND == hr)
	{
		CProtocolBridge::SendEmptyResponse(pHttpContext, 404, _T("Not Found"), hr);
	}
	else if (ERROR_NOT_SUPPORTED == hr)
	{
		if (log)
		{
			log->Log(L"iisnode rejected websocket connection request", WINEVENT_LEVEL_INFO);
		}
		CProtocolBridge::SendEmptyResponse(pHttpContext, 501, _T("Not Implemented"), hr);
	}
	else if (!CProtocolBridge::SendIisnodeError(pHttpContext, hr))
	{
		CProtocolBridge::SendEmptyResponse(pHttpContext, 500, _T("Internal Server Error"), hr);
	}

	return RQ_NOTIFICATION_FINISH_REQUEST;
}

REQUEST_NOTIFICATION_STATUS CNodeHttpModule::OnAsyncCompletion(
	IHttpContext* pHttpContext, DWORD dwNotification, BOOL fPostNotification, IHttpEventProvider* pProvider, IHttpCompletionInfo* pCompletionInfo)
{
	if (NULL != pCompletionInfo && NULL != pHttpContext)
	{
		CNodeHttpStoredContext* ctx = (CNodeHttpStoredContext*)pHttpContext->GetModuleContextContainer()->GetModuleContext(this->applicationManager->GetModuleId());		

		ctx->IncreasePendingAsyncOperationCount();

		WCHAR message[256];
		wsprintfW(message, L"iisnode enters CNodeHttpModule::OnAsyncCompletion callback with request notification status of %d", ctx->GetRequestNotificationStatus());
		this->applicationManager->GetEventProvider()->Log(message, WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

		ASYNC_CONTEXT* async = ctx->GetAsyncContext();
		if (NULL != async->completionProcessor)
		{
			async->completionProcessor(pCompletionInfo->GetCompletionStatus(), pCompletionInfo->GetCompletionBytes(), ctx->GetOverlapped());
		}

		wsprintfW(message, L"iisnode leaves CNodeHttpModule::OnAsyncCompletion callback with request notification status of %d", ctx->GetRequestNotificationStatus());
		this->applicationManager->GetEventProvider()->Log(message, WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

		if (0 == ctx->DecreasePendingAsyncOperationCount()) // decreases ref count increased on entering OnAsyncCompletion
		{
			return ctx->GetRequestNotificationStatus();
		}
		else
		{
			return RQ_NOTIFICATION_PENDING;
		}
	}

	return RQ_NOTIFICATION_CONTINUE;
}
