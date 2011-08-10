#ifndef __CPROTOCOLBRIDGE_H__
#define __CPROTOCOLBRIDGE_H__

static class CProtocolBridge
{
private:

	// utility
	static HRESULT PostponeProcessing(CNodeHttpStoredContext* context, DWORD dueTime);
	static HRESULT SendEmptyResponse(CNodeHttpStoredContext* context, USHORT status, PCTSTR reason, HRESULT hresult = S_OK);

	// processing stages
	static void WINAPI CreateNamedPipeConnection(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);

	static void SendHttpRequestHeaders(CNodeHttpStoredContext* context);
	static void WINAPI SendHttpRequestHeadersCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);

	static void ReadRequestBody(CNodeHttpStoredContext* context);
	static void WINAPI ReadRequestBodyCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);	

	static void SendRequestBody(CNodeHttpStoredContext* context, DWORD chunkLength);
	static void WINAPI SendRequestBodyCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);	

	static void ReadResponseHeaders(CNodeHttpStoredContext* context);

public:

	static HRESULT InitiateRequest(CNodeHttpStoredContext* context);
};

#endif