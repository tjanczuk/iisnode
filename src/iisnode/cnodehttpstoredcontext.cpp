#include "precomp.h"

CNodeHttpStoredContext::CNodeHttpStoredContext(CNodeApplication* nodeApplication, IHttpContext* context)
	: nodeApplication(nodeApplication), context(context)
{
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

void CNodeHttpStoredContext::CleanupStoredContext()
{
	delete this;
}