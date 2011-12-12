#ifndef __CPENDINGREQUESTQUEUE_H__
#define __CPENDINGREQUESTQUEUE_H__

class CNodeHttpStoredContext;

class CPendingRequestQueue
{
private:

	typedef struct _REQUEST_ENTRY {
		SLIST_ENTRY listEntry;
		CNodeHttpStoredContext* context;
	} REQUEST_ENTRY, *PREQUEST_ENTRY;

	PSLIST_HEADER list;
	unsigned long count;

public:

	CPendingRequestQueue();
	~CPendingRequestQueue();

	HRESULT Initialize();
	BOOL IsEmpty();
	HRESULT Push(CNodeHttpStoredContext* context);
	CNodeHttpStoredContext* Pop();

};

#endif