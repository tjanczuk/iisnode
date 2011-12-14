#include "precomp.h"

CNodeHttpModule::CNodeHttpModule(CNodeApplicationManager* applicationManager)
	: applicationManager(applicationManager)
{
}

#if TRUE
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
#else

// This section can be used to establish performance baseline 
// This simple handler returns an HTTP 200 with a Hello, world! in the body

REQUEST_NOTIFICATION_STATUS CNodeHttpModule::OnExecuteRequestHandler(
	IN IHttpContext* pHttpContext, 
	IN IHttpEventProvider* pProvider)
{
        UNREFERENCED_PARAMETER( pProvider );

        // Create an HRESULT to receive return values from methods.
        HRESULT hr;

        // Retrieve a pointer to the response.
        IHttpResponse * pHttpResponse = pHttpContext->GetResponse();

        // Test for an error.
        if (pHttpResponse != NULL)
        {
            // Clear the existing response.
            pHttpResponse->Clear();
            // Set the MIME type to plain text.
            pHttpResponse->SetHeader(
                HttpHeaderContentType,"text/plain",
                (USHORT)strlen("text/plain"),TRUE);

            // Create a string with the response.
            PCSTR pszBuffer = "Hello World!";
            // Create a data chunk.
            HTTP_DATA_CHUNK dataChunk;
            // Set the chunk to a chunk in memory.
            dataChunk.DataChunkType = HttpDataChunkFromMemory;
            // Buffer for bytes written of data chunk.
            DWORD cbSent;

            // Set the chunk to the buffer.
            dataChunk.FromMemory.pBuffer =
                (PVOID) pszBuffer;
            // Set the chunk size to the buffer size.
            dataChunk.FromMemory.BufferLength =
                (USHORT) strlen(pszBuffer);
            // Insert the data chunk into the response.
            hr = pHttpResponse->WriteEntityChunks(
                &dataChunk,1,FALSE,TRUE,&cbSent);

            // Test for an error.
            if (FAILED(hr))
            {
                // Set the HTTP status.
                pHttpResponse->SetStatus(500,"Server Error",0,hr);
            }

            // End additional processing.
            return RQ_NOTIFICATION_FINISH_REQUEST;
        }

        // Return processing to the pipeline.
        return RQ_NOTIFICATION_CONTINUE;
}
#endif

REQUEST_NOTIFICATION_STATUS CNodeHttpModule::OnAsyncCompletion(
	IHttpContext* pHttpContext, DWORD dwNotification, BOOL fPostNotification, IHttpEventProvider* pProvider, IHttpCompletionInfo* pCompletionInfo)
{
	if (NULL != pCompletionInfo && NULL != pHttpContext)
	{
		CNodeHttpStoredContext* ctx = (CNodeHttpStoredContext*)pHttpContext->GetModuleContextContainer()->GetModuleContext(this->applicationManager->GetModuleId());		

		ctx->IncreasePendingAsyncOperationCount();

		this->applicationManager->GetEventProvider()->Log(
			L"iisnode enters CNodeHttpModule::OnAsyncCompletion callback", 
			WINEVENT_LEVEL_VERBOSE, 
			ctx->GetActivityId());

		ASYNC_CONTEXT* async = ctx->GetAsyncContext();
		if (NULL != async->completionProcessor)
		{
			async->completionProcessor(pCompletionInfo->GetCompletionStatus(), pCompletionInfo->GetCompletionBytes(), ctx->GetOverlapped());
		}

		if (0 == ctx->DecreasePendingAsyncOperationCount()) // decreases ref count increased on entering OnAsyncCompletion
		{
			this->applicationManager->GetEventProvider()->Log(
				L"iisnode leaves CNodeHttpModule::OnAsyncCompletion and completes the request", 
				WINEVENT_LEVEL_VERBOSE, 
				ctx->GetActivityId());
			return ctx->GetRequestNotificationStatus();
		}
		else
		{
			this->applicationManager->GetEventProvider()->Log(
				L"iisnode leaves CNodeHttpModule::OnAsyncCompletion and continues the request", 
				WINEVENT_LEVEL_VERBOSE, 
				ctx->GetActivityId());
			return RQ_NOTIFICATION_PENDING;
		}
	}

	return RQ_NOTIFICATION_CONTINUE;
}
