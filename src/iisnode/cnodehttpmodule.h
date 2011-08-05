#ifndef __CNODEHTTPMODULE_H__
#define __CNODEHTTPMODULE_H__

class CNodeApplicationManager;

// TODO, tjanczuk, change CNodeHttpModule to a singleton

class CNodeHttpModule : public CHttpModule
{
private:

	CNodeApplicationManager* applicationManager;

public:

	// Caller owns applicationManagager.
	CNodeHttpModule(CNodeApplicationManager* applicationManager);

	REQUEST_NOTIFICATION_STATUS OnExecuteRequestHandler(IN IHttpContext* pHttpContext, IN IHttpEventProvider* pProvider);

};

#endif
