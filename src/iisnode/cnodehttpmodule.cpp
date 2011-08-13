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

	CheckError(this->applicationManager->StartNewRequest(pHttpContext, pProvider));

	return RQ_NOTIFICATION_PENDING;
Error:

	CProtocolBridge::SendEmptyResponse(pHttpContext, 500, _T("Internal Server Error"), hr, FALSE);

	return RQ_NOTIFICATION_FINISH_REQUEST;
}

REQUEST_NOTIFICATION_STATUS CNodeHttpModule::OnAsyncCompletion(
	IHttpContext* pHttpContext, DWORD dwNotification, BOOL fPostNotification, IHttpEventProvider* pProvider, IHttpCompletionInfo* pCompletionInfo)
{
	if (NULL != pCompletionInfo && NULL != pHttpContext)
	{
		CNodeHttpStoredContext* ctx = (CNodeHttpStoredContext*)pHttpContext->GetModuleContextContainer()->GetModuleContext(this->applicationManager->GetModuleId());
		ASYNC_CONTEXT* async = ctx->GetAsyncContext();
		async->completionProcessor(pCompletionInfo->GetCompletionStatus(), pCompletionInfo->GetCompletionBytes(), ctx->GetOverlapped());
	}

	return RQ_NOTIFICATION_PENDING;
}
