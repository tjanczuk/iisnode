#include "precomp.h"

CNodeHttpStoredContext::CNodeHttpStoredContext(IHttpContext* context)
	: context(context)
{
}

CNodeHttpStoredContext::~CNodeHttpStoredContext()
{
}

void CNodeHttpStoredContext::CleanupStoredContext()
{
	delete this;
}