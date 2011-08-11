#ifndef __CNODEHTTPSTOREDCONTEXT_H__
#define __CNODEHTTPSTOREDCONTEXT_H__

class CNodeApplication;
class CNodeProcess;

class CNodeHttpStoredContext : public IHttpStoredContext
{
private:

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

public:

	// Context is owned by the caller
	CNodeHttpStoredContext(CNodeApplication* nodeApplication, IHttpContext* context);
	~CNodeHttpStoredContext();

	IHttpContext* GetHttpContext();
	CNodeApplication* GetNodeApplication();
	LPOVERLAPPED GetOverlapped();
	CNodeProcess* GetNodeProcess();
	ASYNC_CONTEXT* GetAsyncContext();
	HANDLE GetPipe();
	DWORD GetConnectionRetryCount();
	void* GetBuffer();
	DWORD GetBufferSize();
	void** GetBufferRef();
	DWORD* GetBufferSizeRef();
	DWORD GetDataSize();
	DWORD GetParsingOffset();

	void SetNextProcessor(LPOVERLAPPED_COMPLETION_ROUTINE processor);	
	void SetNodeProcess(CNodeProcess* process);
	void SetPipe(HANDLE pipe);
	void SetConnectionRetryCount(DWORD count);
	void SetBuffer(void* buffer);
	void SetBufferSize(DWORD bufferSize);
	void SetDataSize(DWORD dataSize);
	void SetParsingOffset(DWORD parsingOffet);

	static CNodeHttpStoredContext* Get(LPOVERLAPPED overlapped);

	virtual void CleanupStoredContext();
};

#endif