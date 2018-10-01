#ifndef __CNODEHTTPSTOREDCONTEXT_H__
#define __CNODEHTTPSTOREDCONTEXT_H__

class CNodeApplication;
class CNodeProcess;

class CNodeHttpStoredContext : public IHttpStoredContext
{
private:

	GUID activityId;
	CNodeApplication* nodeApplication;
	IHttpContext* context;
	ASYNC_CONTEXT asyncContext;
	CNodeProcess* process;
	HANDLE pipe;
	DWORD connectionRetryCount;
	void* buffer;
	DWORD bufferSize;
	DWORD dataSize;
	DWORD parsingOffset;
	LONGLONG chunkTransmitted;
	LONGLONG chunkLength;
	BOOL isChunked;
	HRESULT result;
	REQUEST_NOTIFICATION_STATUS requestNotificationStatus;	
	long volatile pendingAsyncOperationCount;
	PCSTR targetUrl;
	DWORD targetUrlLength;
	IHttpContext* childContext;
	BOOL isLastChunk;
	BOOL isConnectionFromPool;
	BOOL expectResponseBody;
	BOOL closeConnection;
	BOOL isUpgrade;
	CNodeHttpStoredContext* upgradeContext;
	BOOL opaqueFlagSet;
	BOOL requestPumpStarted;
	FILETIME startTime;
	HTTP_DATA_CHUNK responseChunk;
	DWORD responseChunkBufferSize;
	CNodeEventProvider* eventProvider;
    ~CNodeHttpStoredContext();

	mutable LONG m_cRefs;

public:

	// Context is owned by the caller
	CNodeHttpStoredContext(CNodeApplication* nodeApplication, CNodeEventProvider* eventProvider, IHttpContext* context);

	VOID
	ReferenceNodeHttpStoredContext(
		VOID
	)
	{
		InterlockedIncrement(&m_cRefs);
	}

	VOID
	DereferenceNodeHttpStoredContext(
		VOID
	)
	{
		_ASSERT(m_cRefs != 0);

		if (InterlockedDecrement(&m_cRefs) == 0)
		{
			delete this;
		}
	}

	IHttpContext* GetHttpContext();
	CNodeApplication* GetNodeApplication();
	LPOVERLAPPED GetOverlapped();
	CNodeProcess* GetNodeProcess();
	ASYNC_CONTEXT* GetAsyncContext();
	HANDLE GetPipe();
	DWORD GetConnectionRetryCount();
	void* GetBuffer();
	DWORD GetBufferSize();
	void* GetChunkBuffer();
	DWORD GetChunkBufferSize();
	DWORD GetChunkHeaderMaxSize();
	void** GetBufferRef();
	DWORD* GetBufferSizeRef();
	DWORD GetDataSize();
	DWORD GetParsingOffset();
	LONGLONG GetChunkTransmitted();
	LONGLONG GetChunkLength();
	BOOL GetIsChunked();
	void SetIsLastChunk(BOOL lastChunk);
	BOOL GetIsLastChunk();
	HRESULT GetHresult();
	REQUEST_NOTIFICATION_STATUS GetRequestNotificationStatus();
	GUID* GetActivityId();
	PCSTR GetTargetUrl();
	DWORD GetTargetUrlLength();
	void SetTargetUrl(PCSTR targetUrl, DWORD targetUrlLength);
	void SetChildContext(IHttpContext* context);
	IHttpContext* GetChildContext();
	DWORD GetBytesCompleted();

	void SetNextProcessor(LPOVERLAPPED_COMPLETION_ROUTINE_IISNODE processor);	
	void SetNodeProcess(CNodeProcess* process);
	void SetPipe(HANDLE pipe);
	void SetConnectionRetryCount(DWORD count);
	void SetBuffer(void* buffer);
	void SetBufferSize(DWORD bufferSize);
	void SetDataSize(DWORD dataSize);
	void SetParsingOffset(DWORD parsingOffet);
	void SetChunkTransmitted(LONGLONG length);
	void SetChunkLength(LONGLONG length);
	void SetIsChunked(BOOL chunked);
	void SetHresult(HRESULT result);
	void SetRequestNotificationStatus(REQUEST_NOTIFICATION_STATUS status);
	LPOVERLAPPED InitializeOverlapped();
	long IncreasePendingAsyncOperationCount();
	long DecreasePendingAsyncOperationCount();
	BOOL GetIsConnectionFromPool();
	void SetIsConnectionFromPool(BOOL fromPool);
	void SetExpectResponseBody(BOOL expect);
	BOOL GetExpectResponseBody();
	void SetCloseConnection(BOOL close);
	BOOL GetCloseConnection();
	HRESULT SetupUpgrade();
	BOOL GetIsUpgrade();
	CNodeHttpStoredContext* GetUpgradeContext(); 
	void SetOpaqueFlag();
	BOOL GetOpaqueFlagSet();
	void SetRequestPumpStarted();
	BOOL GetRequestPumpStarted();
	FILETIME* GetStartTime();
	HRESULT EnsureResponseChunk(DWORD size, HTTP_DATA_CHUNK** chunk);
	void SetBytesCompleted(DWORD bytesCompleted);
	void SetContinueSynchronously(BOOL continueSynchronously);

	static CNodeHttpStoredContext* Get(LPOVERLAPPED overlapped);

	virtual void CleanupStoredContext();
};

#endif