#ifndef __CNODEPROCESSMANAGER_H__
#define __CNODEPROCESSMANAGER_H__

class CNodeProcessManager
{
private:

	CNodeApplication* application;

public:

	CNodeProcessManager(CNodeApplication* application);
	~CNodeProcessManager();

	CNodeApplication* GetApplication();
	HRESULT Initialize();

	void OnNewPendingRequest();
};

#endif