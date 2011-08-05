#ifndef __CNODEAPPLICATION_H__
#define __CNODEAPPLICATION_H__

class CNodeApplication
{
private:

	PWSTR scriptName;

public:

	CNodeApplication();	
	~CNodeApplication();

	HRESULT Initialize(PCWSTR scriptName);

	PCWSTR GetScriptName();
};

#endif