#ifndef __CNODEHTTPMODULE_H__
#define __CNODEHTTPMODULE_H__

class CNodeApplicationManager;

// TODO, tjanczuk, consider changing CNodeHttpModule to a singleton

class CNodeHttpModule : public CHttpModule
{
private:

	CNodeApplicationManager* applicationManager;

public:

	// Caller owns applicationManagager.
	CNodeHttpModule(CNodeApplicationManager* applicationManager);

	REQUEST_NOTIFICATION_STATUS OnExecuteRequestHandler(IN IHttpContext* pHttpContext, IN IHttpEventProvider* pProvider);
	REQUEST_NOTIFICATION_STATUS OnSendResponse(IN IHttpContext* pHttpContext, IN ISendResponseProvider* pProvider);
	REQUEST_NOTIFICATION_STATUS OnAsyncCompletion(IHttpContext* pHttpContext, DWORD dwNotification, BOOL fPostNotification, IHttpEventProvider* pProvider, IHttpCompletionInfo* pCompletionInfo);

};

#endif
