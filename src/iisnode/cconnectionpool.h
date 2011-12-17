#ifndef __CCONNECTIONPOOL_H__
#define __CCONNECTIONPOOL_H__

class CConnectionPool
{
private:

	typedef struct _CONNECTION_ENTRY {
		SLIST_ENTRY listEntry;
		HANDLE connection;
		DWORD timestamp;
	} CONNECTION_ENTRY, *PCONNECTION_ENTRY;

	PSLIST_HEADER list;
	unsigned long count;
	DWORD maxPoolSize;
	DWORD maxAge;

public:

	CConnectionPool();
	~CConnectionPool();

	HRESULT Initialize(IHttpContext* ctx);
	HRESULT Return(HANDLE connection);
	HANDLE Take();

};

#endif