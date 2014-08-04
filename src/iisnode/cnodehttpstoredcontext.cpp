#include "precomp.h"

CNodeHttpStoredContext::CNodeHttpStoredContext(CNodeApplication* nodeApplication, CNodeEventProvider* eventProvider, IHttpContext* context)
    : nodeApplication(nodeApplication), context(context), process(NULL), buffer(NULL), bufferSize(0), dataSize(0), parsingOffset(0),
    chunkLength(0), chunkTransmitted(0), isChunked(FALSE), pipe(INVALID_HANDLE_VALUE), result(S_OK), isLastChunk(FALSE),
    requestNotificationStatus(RQ_NOTIFICATION_PENDING), connectionRetryCount(0), pendingAsyncOperationCount(1),
    targetUrl(NULL), targetUrlLength(0), childContext(NULL), isConnectionFromPool(FALSE), expectResponseBody(TRUE),
    closeConnection(FALSE), isUpgrade(FALSE), upgradeContext(NULL), opaqueFlagSet(FALSE), requestPumpStarted(FALSE),
    responseChunkBufferSize(0)
{
    IHttpTraceContext* tctx;
    LPCGUID pguid;

    this->responseChunk.DataChunkType = HttpDataChunkFromMemory;
    this->responseChunk.FromMemory.pBuffer = NULL;

    RtlZeroMemory(&this->asyncContext, sizeof(ASYNC_CONTEXT));
    CoCreateGuid(&this->activityId);
    
    this->asyncContext.data = this;
    this->eventProvider = eventProvider;
}

CNodeHttpStoredContext::~CNodeHttpStoredContext()
{
    if (NULL != this->upgradeContext)
    {
        delete this->upgradeContext;
        this->upgradeContext = NULL;
    }
    else if (INVALID_HANDLE_VALUE != this->pipe)
    {
        CloseHandle(this->pipe);
        this->pipe = INVALID_HANDLE_VALUE;
    }

    if (this->responseChunk.FromMemory.pBuffer) {
        free(this->responseChunk.FromMemory.pBuffer);
        this->responseChunk.FromMemory.pBuffer = NULL;
        responseChunkBufferSize = 0;
    }
}

HRESULT CNodeHttpStoredContext::EnsureResponseChunk(DWORD size, HTTP_DATA_CHUNK** chunk)
{
    HRESULT hr;

    if (size > this->responseChunkBufferSize)
    {
        if (this->responseChunk.FromMemory.pBuffer) 
        {
            free(this->responseChunk.FromMemory.pBuffer);
            this->responseChunk.FromMemory.pBuffer = NULL;
            this->responseChunkBufferSize = 0;
        }

        ErrorIf(NULL == (this->responseChunk.FromMemory.pBuffer = malloc(size)), ERROR_NOT_ENOUGH_MEMORY);
        this->responseChunkBufferSize = size;
    }

    *chunk = &this->responseChunk;

    return S_OK;
Error:
    return hr;
}

IHttpContext* CNodeHttpStoredContext::GetHttpContext() 
{ 
    return this->context; 
}

CNodeApplication* CNodeHttpStoredContext::GetNodeApplication() 
{ 
    return this->nodeApplication; 
}

void CNodeHttpStoredContext::SetNextProcessor(LPOVERLAPPED_COMPLETION_ROUTINE processor)
{
    this->asyncContext.completionProcessor = processor;
    this->SetContinueSynchronously(FALSE);
}

LPOVERLAPPED CNodeHttpStoredContext::GetOverlapped()
{
    return &this->asyncContext.overlapped;
}

LPOVERLAPPED CNodeHttpStoredContext::InitializeOverlapped()
{
    RtlZeroMemory(&this->asyncContext.overlapped, sizeof(OVERLAPPED));

    return &this->asyncContext.overlapped;
}

void CNodeHttpStoredContext::CleanupStoredContext()
{	
    delete this;
}

CNodeProcess* CNodeHttpStoredContext::GetNodeProcess()
{
    return this->process;
}

void CNodeHttpStoredContext::SetNodeProcess(CNodeProcess* process)
{
    this->process = process;
}

ASYNC_CONTEXT* CNodeHttpStoredContext::GetAsyncContext()
{
    return &this->asyncContext;
}

CNodeHttpStoredContext* CNodeHttpStoredContext::Get(LPOVERLAPPED overlapped)
{
    return overlapped == NULL ? NULL : (CNodeHttpStoredContext*)((ASYNC_CONTEXT*)overlapped)->data;
}

HANDLE CNodeHttpStoredContext::GetPipe()
{
    return this->pipe;
}

void CNodeHttpStoredContext::SetPipe(HANDLE pipe)
{
    this->pipe = pipe;
    if (NULL != this->upgradeContext)
    {
        this->upgradeContext->SetPipe(pipe);
    }
}

DWORD CNodeHttpStoredContext::GetConnectionRetryCount()
{
    return this->connectionRetryCount;
}

void CNodeHttpStoredContext::SetConnectionRetryCount(DWORD count)
{
    this->connectionRetryCount = count;
}

void* CNodeHttpStoredContext::GetBuffer()
{
    return this->buffer;
}

DWORD CNodeHttpStoredContext::GetBufferSize()
{
    return this->bufferSize;
}

void* CNodeHttpStoredContext::GetChunkBuffer()
{
    // leave room in the allocated memory buffer for a chunk transfer encoding header that 
    // will be calculated only after the entity body chunk had been read

    return (void*)((char*)this->GetBuffer() + this->GetChunkHeaderMaxSize());
}

DWORD CNodeHttpStoredContext::GetChunkBufferSize()
{
    // leave room in the buffer for the chunk header and the CRLF following a chunk

    return this->GetBufferSize() - this->GetChunkHeaderMaxSize() - 2;
}

DWORD CNodeHttpStoredContext::GetChunkHeaderMaxSize() 
{
    // the maximum size of the chunk header

    return 64;
}

void** CNodeHttpStoredContext::GetBufferRef()
{
    return &this->buffer;
}

DWORD* CNodeHttpStoredContext::GetBufferSizeRef()
{
    return &this->bufferSize;
}

void CNodeHttpStoredContext::SetBuffer(void* buffer)
{
    this->buffer = buffer;
}

void CNodeHttpStoredContext::SetBufferSize(DWORD bufferSize)
{
    this->bufferSize = bufferSize;
}

DWORD CNodeHttpStoredContext::GetDataSize()
{
    return this->dataSize;
}

DWORD CNodeHttpStoredContext::GetParsingOffset()
{
    return this->parsingOffset;
}

void CNodeHttpStoredContext::SetDataSize(DWORD dataSize)
{
    this->dataSize = dataSize;
}

void CNodeHttpStoredContext::SetParsingOffset(DWORD parsingOffset)
{
    this->parsingOffset = parsingOffset;
}

LONGLONG CNodeHttpStoredContext::GetChunkTransmitted()
{
    return this->chunkTransmitted;
}

LONGLONG CNodeHttpStoredContext::GetChunkLength()
{
    return this->chunkLength;
}

void CNodeHttpStoredContext::SetChunkTransmitted(LONGLONG length)
{
    this->chunkTransmitted = length;
}

void CNodeHttpStoredContext::SetChunkLength(LONGLONG length)
{
    this->chunkLength = length;
}

BOOL CNodeHttpStoredContext::GetIsChunked()
{
    return this->isChunked;
}

void CNodeHttpStoredContext::SetIsChunked(BOOL chunked)
{
    this->isChunked = chunked;
}

void CNodeHttpStoredContext::SetIsLastChunk(BOOL lastChunk)
{
    this->isLastChunk = lastChunk;
}

BOOL CNodeHttpStoredContext::GetIsLastChunk()
{
    return this->isLastChunk;
}

HRESULT CNodeHttpStoredContext::GetHresult()
{
    return this->result;
}

void CNodeHttpStoredContext::SetHresult(HRESULT result)
{
    this->result = result;
}

REQUEST_NOTIFICATION_STATUS CNodeHttpStoredContext::GetRequestNotificationStatus()
{
    return this->requestNotificationStatus;
}

void CNodeHttpStoredContext::SetRequestNotificationStatus(REQUEST_NOTIFICATION_STATUS status)
{
    this->requestNotificationStatus = status;
}

GUID* CNodeHttpStoredContext::GetActivityId()
{
    return &this->activityId;
}

long CNodeHttpStoredContext::IncreasePendingAsyncOperationCount()
{
    this->eventProvider->Log(this->context,
        L"iisnode increases pending async operation count", 
        WINEVENT_LEVEL_VERBOSE, 
        this->GetActivityId());
    if (this->requestPumpStarted)
    {
        return InterlockedIncrement(&this->upgradeContext->pendingAsyncOperationCount);
    }
    else
    {
        return InterlockedIncrement(&this->pendingAsyncOperationCount);
    }
}

long CNodeHttpStoredContext::DecreasePendingAsyncOperationCount()
{
    this->eventProvider->Log(this->context,
        L"iisnode decreases pending async operation count", 
        WINEVENT_LEVEL_VERBOSE, 
        this->GetActivityId());
    if (this->requestPumpStarted)
    {
        return InterlockedDecrement(&this->upgradeContext->pendingAsyncOperationCount);
    }
    else
    {
        return InterlockedDecrement(&this->pendingAsyncOperationCount);
    }
}

PCSTR CNodeHttpStoredContext::GetTargetUrl()
{
    return this->targetUrl;
}

DWORD CNodeHttpStoredContext::GetTargetUrlLength()
{
    return this->targetUrlLength;
}

void CNodeHttpStoredContext::SetTargetUrl(PCSTR targetUrl, DWORD targetUrlLength)
{
    this->targetUrl = targetUrl;
    this->targetUrlLength = targetUrlLength;
}

void CNodeHttpStoredContext::SetChildContext(IHttpContext* context)
{
    this->childContext = context;
}

IHttpContext* CNodeHttpStoredContext::GetChildContext()
{
    return this->childContext;
}

BOOL CNodeHttpStoredContext::GetIsConnectionFromPool()
{
    return this->isConnectionFromPool;
}

void CNodeHttpStoredContext::SetIsConnectionFromPool(BOOL fromPool)
{
    this->isConnectionFromPool = fromPool;
}

void CNodeHttpStoredContext::SetExpectResponseBody(BOOL expect)
{
    this->expectResponseBody = expect;
}

BOOL CNodeHttpStoredContext::GetExpectResponseBody()
{
    return this->expectResponseBody;
}

void CNodeHttpStoredContext::SetCloseConnection(BOOL close)
{
    this->closeConnection = close;
}

BOOL CNodeHttpStoredContext::GetCloseConnection()
{
    return this->closeConnection;
}

HRESULT CNodeHttpStoredContext::SetupUpgrade()
{
    HRESULT hr;

    ErrorIf(this->isUpgrade, E_FAIL);

    // The upgradeContext is used to pump incoming bytes to the node.js application. 
    // The 'this' context is used to pump outgoing bytes to IIS. Once the response headers are flushed,
    // both contexts are used concurrently in a full duplex, asynchronous fashion. 
    // The last context to complete pumping closes the IIS request. 

    ErrorIf(NULL == (this->upgradeContext = new CNodeHttpStoredContext(this->GetNodeApplication(), this->eventProvider, this->GetHttpContext())), 
        ERROR_NOT_ENOUGH_MEMORY);	
    this->upgradeContext->bufferSize = CModuleConfiguration::GetInitialRequestBufferSize(this->context);
    ErrorIf(NULL == (this->upgradeContext->buffer = this->context->AllocateRequestMemory(this->upgradeContext->bufferSize)),
        ERROR_NOT_ENOUGH_MEMORY);	

    // Enable duplex read/write of data 

    IHttpContext3* ctx3 = (IHttpContext3*)this->GetHttpContext();
    ctx3->EnableFullDuplex();

    // Disable caching and buffering

    ctx3->GetResponse()->DisableBuffering();
    ctx3->GetResponse()->DisableKernelCache();

    this->upgradeContext->SetPipe(this->GetPipe());
    this->upgradeContext->SetNodeProcess(this->GetNodeProcess());
    this->upgradeContext->isUpgrade = TRUE;
    this->isUpgrade = TRUE;

    return S_OK;

Error:

    return hr;
}

BOOL CNodeHttpStoredContext::GetIsUpgrade()
{
    return this->isUpgrade;
}

CNodeHttpStoredContext* CNodeHttpStoredContext::GetUpgradeContext() 
{
    return this->upgradeContext;
}

void CNodeHttpStoredContext::SetOpaqueFlag()
{
    this->opaqueFlagSet = TRUE;
}

BOOL CNodeHttpStoredContext::GetOpaqueFlagSet()
{
    return this->opaqueFlagSet;
}

void CNodeHttpStoredContext::SetRequestPumpStarted()
{
    // The pending async operation count for the pair of CNodeHttpStoredContexts will be maintained in the upgradeContext instance from now on.
    // The +1 represents the creation of the upgradeContext and the corresponding decrease happens when the pumping of incoming bytes completes.
    this->upgradeContext->pendingAsyncOperationCount = this->pendingAsyncOperationCount + 1;
    this->pendingAsyncOperationCount = 0;
    this->requestPumpStarted = TRUE;
}

BOOL CNodeHttpStoredContext::GetRequestPumpStarted() 
{
    return this->requestPumpStarted;
}

FILETIME* CNodeHttpStoredContext::GetStartTime()
{
    return &this->startTime;
}

DWORD CNodeHttpStoredContext::GetBytesCompleted()
{
    return this->asyncContext.bytesCompleteted;
}

void CNodeHttpStoredContext::SetBytesCompleted(DWORD bytesCompleted)
{
    this->asyncContext.bytesCompleteted = bytesCompleted;
}

void CNodeHttpStoredContext::SetContinueSynchronously(BOOL continueSynchronously)
{
    this->asyncContext.continueSynchronously = continueSynchronously;
}
