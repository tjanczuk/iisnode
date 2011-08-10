#ifndef __CPENDINGREQUESTQUEUE_H__
#define __CPENDINGREQUESTQUEUE_H__

class CNodeHttpStoredContext;

typedef std::queue<CNodeHttpStoredContext*> CNodeHttpStoredContextQueue;

class CPendingRequestQueue
{
private:

	CNodeHttpStoredContextQueue requests;
	CRITICAL_SECTION syncRoot;

public:

	CPendingRequestQueue();
	~CPendingRequestQueue();

	BOOL IsEmpty();
	HRESULT Push(CNodeHttpStoredContext* context);
	CNodeHttpStoredContext* Peek();
	void Pop();

};

#endif