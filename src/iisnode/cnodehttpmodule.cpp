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

	return RQ_NOTIFICATION_CONTINUE;
Error:
	return RQ_NOTIFICATION_FINISH_REQUEST;
}
