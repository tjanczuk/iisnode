#include "precomp.h"

//  Implementation of the OnAcquireRequestState method
REQUEST_NOTIFICATION_STATUS
CNodeHttpModule::OnAcquireRequestState(
    IN IHttpContext *                       pHttpContext,
    IN OUT IHttpEventProvider *             pProvider
)
{
    HRESULT                         hr = S_OK;

	// TODO: implement the AcquireRequestState module functionality

Finished:

    if ( FAILED( hr )  )
    {
        return RQ_NOTIFICATION_FINISH_REQUEST;
    }
    else
    {
        return RQ_NOTIFICATION_CONTINUE;
    }
}

// TODO: implement other desired event handler methods below