#ifndef __CACTIVEREQUESTPOOL_H__
#define __CACTIVEREQUESTPOOL_H__

class CNodeHttpStoredContext;

class CActiveRequestPool
{
private: 
	
	DWORD requestCount;
	CRITICAL_SECTION syncRoot;
	HANDLE drainHandle;

public:

	CActiveRequestPool();
	~CActiveRequestPool();

	HRESULT Add(CNodeHttpStoredContext* context);
	HRESULT Remove();
	void SignalWhenDrained(HANDLE drainHandle);
	DWORD GetRequestCount();
};

#endif