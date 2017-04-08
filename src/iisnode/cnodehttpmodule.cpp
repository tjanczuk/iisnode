#include "precomp.h"

CNodeHttpModule::CNodeHttpModule(CNodeApplicationManager* applicationManager)
    : applicationManager(applicationManager)
{
}

REQUEST_NOTIFICATION_STATUS CNodeHttpModule::OnSendResponse(IN IHttpContext* pHttpContext, IN ISendResponseProvider* pProvider)
{
    if (NULL != pHttpContext && NULL != pProvider)
    {
        CNodeHttpStoredContext* ctx = (CNodeHttpStoredContext*)pHttpContext->GetModuleContextContainer()->GetModuleContext(this->applicationManager->GetModuleId());
        DWORD flags = pProvider->GetFlags();
        if (NULL != ctx && ctx->GetIsUpgrade() && !ctx->GetOpaqueFlagSet())
        {
            // Set opaque mode in HTTP.SYS to enable exchanging raw bytes.
            
            pProvider->SetFlags(flags | HTTP_SEND_RESPONSE_FLAG_OPAQUE);
            ctx->SetOpaqueFlag();
        }
    }

    return RQ_NOTIFICATION_CONTINUE;
}

#if TRUE
REQUEST_NOTIFICATION_STATUS CNodeHttpModule::OnExecuteRequestHandler(
    IN IHttpContext* pHttpContext, 
    IN IHttpEventProvider* pProvider)
{
    HRESULT hr;
    CNodeHttpStoredContext* ctx = NULL;

    CheckError(this->applicationManager->Initialize(pHttpContext));

    this->applicationManager->GetEventProvider()->Log(pHttpContext, L"iisnode received a new http request", WINEVENT_LEVEL_INFO);

    CheckError(this->applicationManager->Dispatch(pHttpContext, pProvider, &ctx));
    this->applicationManager->GetEventProvider()->Log(pHttpContext, L"iisnode dispatched new http request", WINEVENT_LEVEL_INFO, ctx->GetActivityId());
    ASYNC_CONTEXT* async = ctx->GetAsyncContext();
    async->RunSynchronousContinuations();

    REQUEST_NOTIFICATION_STATUS result;
    if (0 == ctx->DecreasePendingAsyncOperationCount()) // decreases ref count set to 1 in the ctor of CNodeHttpStoredContext
    {
        result = ctx->GetRequestNotificationStatus();
    }
    else
    {
        result = RQ_NOTIFICATION_PENDING;
    }

    switch (result) 
    {
    default:
        this->applicationManager->GetEventProvider()->Log(pHttpContext,
            L"iisnode leaves CNodeHttpModule::OnExecuteRequestHandler", 
            WINEVENT_LEVEL_VERBOSE, 
            ctx->GetActivityId());
        break;
    case RQ_NOTIFICATION_CONTINUE:
        this->applicationManager->GetEventProvider()->Log(pHttpContext,
            L"iisnode leaves CNodeHttpModule::OnExecuteRequestHandler with RQ_NOTIFICATION_CONTINUE", 
            WINEVENT_LEVEL_VERBOSE, 
            ctx->GetActivityId());
        break;
    case RQ_NOTIFICATION_FINISH_REQUEST:
        this->applicationManager->GetEventProvider()->Log(pHttpContext,
            L"iisnode leaves CNodeHttpModule::OnExecuteRequestHandler with RQ_NOTIFICATION_FINISH_REQUEST", 
            WINEVENT_LEVEL_VERBOSE, 
            ctx->GetActivityId());
        break;
    case RQ_NOTIFICATION_PENDING:
        this->applicationManager->GetEventProvider()->Log(pHttpContext,
            L"iisnode leaves CNodeHttpModule::OnExecuteRequestHandler with RQ_NOTIFICATION_PENDING", 
            WINEVENT_LEVEL_VERBOSE, 
            ctx->GetActivityId());
        break;
    };

    return result;

Error:

    CNodeEventProvider* log = this->applicationManager->GetEventProvider();

    if (log)
    {
        if (ctx)
        {
            log->Log(pHttpContext,L"iisnode failed to process a new http request", WINEVENT_LEVEL_INFO, ctx->GetActivityId());
        }
        else
        {
            log->Log(pHttpContext,L"iisnode failed to process a new http request", WINEVENT_LEVEL_INFO);
        }
    }

    if (ERROR_NOT_ENOUGH_QUOTA == hr)
    {
        CProtocolBridge::SendEmptyResponse(pHttpContext, 503, CNodeConstants::IISNODE_ERROR_NOT_ENOUGH_QUOTA, _T("Service Unavailable"), hr);
    }
    else if (ERROR_FILE_NOT_FOUND == hr)
    {
        CProtocolBridge::SendEmptyResponse(pHttpContext, 404, 0, _T("Not Found"), hr);
    }
    else if (ERROR_NOT_SUPPORTED == hr)
    {
        if (log)
        {
            log->Log(pHttpContext, L"iisnode rejected websocket connection request", WINEVENT_LEVEL_INFO);
        }
        CProtocolBridge::SendEmptyResponse(pHttpContext, 501, 0, _T("Not Implemented"), hr);
    }
    else if (!CProtocolBridge::SendIisnodeError(pHttpContext, hr))
    {
        CProtocolBridge::SendEmptyResponse( pHttpContext, 
                                            500, 
                                            CNodeConstants::IISNODE_ERROR_ON_EXECUTE_REQ_HANDLER, 
                                            _T("Internal Server Error"), 
                                            hr );
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

        if (ctx->GetIsUpgrade())
        {
            IHttpCompletionInfo2* pCompletionInfo2 = (IHttpCompletionInfo2*)pCompletionInfo;
            if (1 == pCompletionInfo2->GetCompletedOperation()) 
            {
                // This is completion of the read request for incoming bytes of an opaque byte stream after 101 Switching protocol response was sent

                ctx = ctx->GetUpgradeContext();
            }
        }

        ctx->IncreasePendingAsyncOperationCount();

        this->applicationManager->GetEventProvider()->Log(pHttpContext,
            L"iisnode enters CNodeHttpModule::OnAsyncCompletion callback", 
            WINEVENT_LEVEL_VERBOSE, 
            ctx->GetActivityId());

        ASYNC_CONTEXT* async = ctx->GetAsyncContext();
        if (NULL != async->completionProcessor)
        {
            DWORD bytesCompleted = pCompletionInfo->GetCompletionBytes();
            if (async->completionProcessor == CProtocolBridge::SendResponseBodyCompleted)
            {
                bytesCompleted = async->bytesCompleteted;
                async->bytesCompleteted = 0;
            }
            async->completionProcessor(pCompletionInfo->GetCompletionStatus(), bytesCompleted, ctx->GetOverlapped());
            async->RunSynchronousContinuations();
        }

        long value = ctx->DecreasePendingAsyncOperationCount();

        REQUEST_NOTIFICATION_STATUS result = ctx->GetRequestNotificationStatus();

        if(ctx->GetIsUpgrade() && value == 0)
        {
            ctx->GetNodeProcess()->OnRequestCompleted( ctx );
            //
            // when the pending async count reaches 0,
            // need to return RQ_NOTIFICATION_CONTINUE 
            // to indicate websocket connection close.
            //
            result = RQ_NOTIFICATION_CONTINUE;
        }

        switch (result) 
        {
        default:
            this->applicationManager->GetEventProvider()->Log(pHttpContext,
                L"iisnode leaves CNodeHttpModule::OnAsyncCompletion", 
                WINEVENT_LEVEL_VERBOSE, 
                ctx->GetActivityId());
            break;
        case RQ_NOTIFICATION_CONTINUE:
            this->applicationManager->GetEventProvider()->Log(pHttpContext,
                L"iisnode leaves CNodeHttpModule::OnAsyncCompletion with RQ_NOTIFICATION_CONTINUE", 
                WINEVENT_LEVEL_VERBOSE, 
                ctx->GetActivityId());
            break;
        case RQ_NOTIFICATION_FINISH_REQUEST:
            this->applicationManager->GetEventProvider()->Log(pHttpContext,
                L"iisnode leaves CNodeHttpModule::OnAsyncCompletion with RQ_NOTIFICATION_FINISH_REQUEST", 
                WINEVENT_LEVEL_VERBOSE, 
                ctx->GetActivityId());
            break;
        case RQ_NOTIFICATION_PENDING:
            this->applicationManager->GetEventProvider()->Log(pHttpContext,
                L"iisnode leaves CNodeHttpModule::OnAsyncCompletion with RQ_NOTIFICATION_PENDING", 
                WINEVENT_LEVEL_VERBOSE, 
                ctx->GetActivityId());
            break;
        };

        return result;
    }

    return RQ_NOTIFICATION_CONTINUE;
}
