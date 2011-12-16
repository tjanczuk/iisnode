#include "precomp.h"

CNodeHttpStoredContext::CNodeHttpStoredContext(CNodeApplication* nodeApplication, IHttpContext* context)
	: nodeApplication(nodeApplication), context(context), process(NULL), buffer(NULL), bufferSize(0), dataSize(0), parsingOffset(0),
	chunkLength(0), chunkTransmitted(0), isChunked(FALSE), pipe(INVALID_HANDLE_VALUE), result(S_OK), isLastChunk(FALSE),
	requestNotificationStatus(RQ_NOTIFICATION_PENDING), connectionRetryCount(0), pendingAsyncOperationCount(1),
	targetUrl(NULL), targetUrlLength(0), childContext(NULL)
{
	IHttpTraceContext* tctx;
	LPCGUID pguid;

	RtlZeroMemory(&this->asyncContext, sizeof(ASYNC_CONTEXT));
	if (NULL != (tctx = context->GetTraceContext()) && NULL != (pguid = tctx->GetTraceActivityId()))
	{
		memcpy(&this->activityId, pguid, sizeof GUID);
	}
	else
	{
		CoCreateGuid(&this->activityId);
	}
	
	this->asyncContext.data = this;
}

CNodeHttpStoredContext::~CNodeHttpStoredContext()
{
	if (INVALID_HANDLE_VALUE != this->pipe)
	{
		CloseHandle(this->pipe);
		this->pipe = INVALID_HANDLE_VALUE;
	}
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
	return InterlockedIncrement(&this->pendingAsyncOperationCount);
}

long CNodeHttpStoredContext::DecreasePendingAsyncOperationCount()
{
	return InterlockedDecrement(&this->pendingAsyncOperationCount);
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
