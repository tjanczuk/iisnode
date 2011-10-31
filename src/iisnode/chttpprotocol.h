#ifndef __CHTTPPROTOCOL_H__
#define __CHTTPPROTOCOL_H__

class CNodeHttpStoredContext;

class CHttpProtocol
{
private:

	static PCSTR httpRequestHeaders[HttpHeaderRequestMaximum];
	static HRESULT Append(IHttpContext* context, const char* content, DWORD contentLength, void** buffer, DWORD* bufferLength, DWORD* offset);

public:

	static HRESULT SerializeRequestHeaders(CNodeHttpStoredContext* ctx, void** result, DWORD* resultSize, DWORD* resultLength);
	static HRESULT ParseResponseStatusLine(CNodeHttpStoredContext* context);
	static HRESULT ParseResponseHeaders(CNodeHttpStoredContext* context);
};

#endif