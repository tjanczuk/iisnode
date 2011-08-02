#ifndef __CNODEHTTPMODULE_H__
#define __CNODEHTTPMODULE_H__

//  The module implementation.
//  This class is responsible for implementing the 
//  module functionality for each of the server events
//  that it registers for.
class CNodeHttpModule : public CHttpModule
{
public:

	//  Implementation of the AcquireRequestState event handler method.
    REQUEST_NOTIFICATION_STATUS
    OnAcquireRequestState(
        IN IHttpContext *                       pHttpContext,
        IN OUT IHttpEventProvider *             pProvider
    );

	//  TODO: override additional event handler methods below.
};

#endif
