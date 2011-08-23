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

	CheckError(this->applicationManager->Initialize(pHttpContext));
	CheckError(this->applicationManager->Dispatch(pHttpContext, pProvider));

	return RQ_NOTIFICATION_PENDING;
Error:

	if (ERROR_NOT_ENOUGH_QUOTA == hr)
	{
		CProtocolBridge::SendEmptyResponse(pHttpContext, 503, _T("Service Unavailable"), hr);
	}
	else if (ERROR_FILE_NOT_FOUND == hr)
	{
		CProtocolBridge::SendEmptyResponse(pHttpContext, 404, _T("Not Found"), hr);
	}
	else
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
		ASYNC_CONTEXT* async = ctx->GetAsyncContext();
		if (NULL != async->completionProcessor)
		{
			ctx->SetSynchronous(TRUE);
			async->completionProcessor(pCompletionInfo->GetCompletionStatus(), pCompletionInfo->GetCompletionBytes(), ctx->GetOverlapped());
		}

		return ctx->GetRequestNotificationStatus();
	}

	return RQ_NOTIFICATION_CONTINUE;
}
