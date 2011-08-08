#include "precomp.h"

CNodeHttpStoredContext::CNodeHttpStoredContext(CNodeApplication* nodeApplication, IHttpContext* context)
	: nodeApplication(nodeApplication), context(context)
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