#include "precomp.h"

CNodeHttpStoredContext::CNodeHttpStoredContext(CNodeApplication* nodeApplication, IHttpContext* context)
	: nodeApplication(nodeApplication), context(context), process(NULL), buffer(NULL), bufferSize(0)
{
	RtlZeroMemory(&this->asyncContext, sizeof(ASYNC_CONTEXT));
	this->asyncContext.data = this;
}

CNodeHttpStoredContext::~CNodeHttpStoredContext()
{
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