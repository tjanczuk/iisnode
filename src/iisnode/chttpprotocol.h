#ifndef __CHTTPPROTOCOL_H__
#define __CHTTPPROTOCOL_H__

static class CHttpProtocol
{
private:

	static PCSTR httpRequestHeaders[HttpHeaderRequestMaximum];
	static HRESULT Append(IHttpContext* context, const char* content, DWORD contentLength, void** buffer, DWORD* bufferLength, DWORD* offset);

public:

	static HRESULT SerializeRequestHeaders(IHttpContext* context, void** result, DWORD* resultSize, DWORD* resultLength);
};

#endif