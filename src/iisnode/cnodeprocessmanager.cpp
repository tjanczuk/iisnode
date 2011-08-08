#include "precomp.h"

CNodeProcessManager::CNodeProcessManager(CNodeApplication* application)
	: application(application)
{
}

CNodeProcessManager::~CNodeProcessManager()
{
}

CNodeApplication* CNodeProcessManager::GetApplication()
{
	return this->application;
}

HRESULT CNodeProcessManager::Initialize()
{
	return S_OK;
}

void CNodeProcessManager::OnNewPendingRequest()
{
}