#include "precomp.h"

CNodeApplication::CNodeApplication()
	: scriptName(NULL)
{
}

CNodeApplication::~CNodeApplication()
{
	if (NULL != this->scriptName)
	{
		delete [] this->scriptName;
		this->scriptName = NULL;
	}
}

HRESULT CNodeApplication::Initialize(PCWSTR scriptName)
{
	HRESULT hr;

	ErrorIf(NULL == scriptName, ERROR_INVALID_PARAMETER);

	DWORD len = wcslen(scriptName);
	ErrorIf(NULL == (this->scriptName = new WCHAR[len + 1]), ERROR_NOT_ENOUGH_MEMORY);
	memcpy(this->scriptName, scriptName, sizeof(WCHAR) * len);
	this->scriptName[sizeof(WCHAR) * len] = L'\0';

	return S_OK;
Error:
	return hr;
}

PCWSTR CNodeApplication::GetScriptName()
{
	return this->scriptName;
}