#ifndef __CPROTOCOLBRIDGE_H__
#define __CPROTOCOLBRIDGE_H__

class CProtocolBridge
{
private:

	// utility
	static HRESULT PostponeProcessing(CNodeHttpStoredContext* context, DWORD dueTime);	
	static HRESULT EnsureBuffer(CNodeHttpStoredContext* context);

	// processing stages
	static void WINAPI CreateNamedPipeConnection(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);

	static void SendHttpRequestHeaders(CNodeHttpStoredContext* context);
	static void WINAPI SendHttpRequestHeadersCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);

	static void ReadRequestBody(CNodeHttpStoredContext* context);
	static void WINAPI ReadRequestBodyCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);	

	static void SendRequestBody(CNodeHttpStoredContext* context, DWORD chunkLength);
	static void WINAPI SendRequestBodyCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);	

	static void StartReadResponse(CNodeHttpStoredContext* context);
	static void ContinueReadResponse(CNodeHttpStoredContext* context);
	static void WINAPI ProcessResponseStatusLine(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);	
	static void WINAPI ProcessResponseHeaders(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);	
	static void WINAPI ProcessResponseBody(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);	
	static void WINAPI SendResponseBodyCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);	

	static void FinalizeResponse(CNodeHttpStoredContext* context);

public:

	static HRESULT InitiateRequest(CNodeHttpStoredContext* context);
	static HRESULT SendEmptyResponse(CNodeHttpStoredContext* context, USHORT status, PCTSTR reason, HRESULT hresult);
	static HRESULT SendEmptyResponse(IHttpContext* httpCtx, USHORT status, PCTSTR reason, HRESULT hresult, BOOL indicateCompletion);

};

#endif