#ifndef __CPROTOCOLBRIDGE_H__
#define __CPROTOCOLBRIDGE_H__

class CNodeEventProvider;
class CNodeHttpStoredContext;

class CProtocolBridge
{
private:

	// utility
	static HRESULT PostponeProcessing(CNodeHttpStoredContext* context, DWORD dueTime);	
	static HRESULT EnsureBuffer(CNodeHttpStoredContext* context);
	static HRESULT FinalizeResponseCore(CNodeHttpStoredContext * context, REQUEST_NOTIFICATION_STATUS status, HRESULT error, CNodeEventProvider* log, PCWSTR etw, UCHAR level);
	static BOOL IsLocalCall(IHttpContext* ctx);
	static BOOL SendDevError(CNodeHttpStoredContext* context, USHORT status, USHORT subStatus, PCTSTR reason, HRESULT hresult, BOOL disableCache = FALSE);
	static HRESULT AddDebugHeader(CNodeHttpStoredContext* context);

	// processing stages
	static void WINAPI ChildContextCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);
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

	static void WINAPI ProcessChunkHeader(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);
	static void WINAPI ProcessResponseBody(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);	

	static void WINAPI ProcessUpgradeResponse(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);
	static void WINAPI ContinueProcessResponseBodyAfterPartialFlush(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);	

	static void EnsureRequestPumpStarted(CNodeHttpStoredContext* context);

	static void FinalizeResponse(CNodeHttpStoredContext* context);
	static void FinalizeUpgradeResponse(CNodeHttpStoredContext* context, HRESULT hresult);

public:

	static void WINAPI SendResponseBodyCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped);

	static HRESULT InitiateRequest(CNodeHttpStoredContext* context);
	static BOOL SendIisnodeError(IHttpContext* httpCtx, HRESULT hr);
	static BOOL SendIisnodeError(CNodeHttpStoredContext* ctx, HRESULT hr);
	static HRESULT SendEmptyResponse(CNodeHttpStoredContext* context, USHORT status, USHORT subStatus, PCTSTR reason, HRESULT hresult, BOOL disableCache = FALSE);
	static HRESULT SendSyncResponse(IHttpContext* httpCtx, USHORT status, PCTSTR reason, HRESULT hresult, BOOL disableCache, PCSTR htmlBody);
	static void SendEmptyResponse(IHttpContext* httpCtx, USHORT status, USHORT subStatus, PCTSTR reason, HRESULT hresult, BOOL disableCache = FALSE);
	static HRESULT SendDebugRedirect(CNodeHttpStoredContext* context, CNodeEventProvider* log);

};

#endif