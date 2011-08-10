#include "precomp.h"

PCSTR CHttpProtocol::httpRequestHeaders[] = {
	"Cache-Control",
	"Connection",
	"Date",
	"Keep-Alive",
	"Pragma",
	"Trailer",
	"Transfer-Encoding",
	"Upgrade",
	"Via",
	"Warning",
	"Allow",
	"Content-Length",
	"Content-Type",
	"Content-Encoding",
	"Content-Language",
	"Content-Location",
	"Content-MD5",
	"Content-Range",
	"Expires",
	"Last-Modified",
	"Accept",
	"Accept-Charset",
	"Accept-Encoding",
	"Accept-Language",
	"Authorization",
	"Cookie",
	"Expect",
	"From",
	"Host",
	"If-Match",
	"If-Modified-Since",
	"If-None-Match",
	"If-Range",
	"If-Unmodified-Since",
	"Max-Forwards",
	"Proxy-Authorization",
	"Referer",
	"Range",
	"Te",
	"Translate",
	"User-Agent"
};

HRESULT CHttpProtocol::Append(IHttpContext* context, const char* content, DWORD contentLength, void** buffer, DWORD* bufferLength, DWORD* offset)
{
	HRESULT hr;

	if (contentLength < 0)
	{
		contentLength = strlen(content);
	}

	if ((contentLength + *offset) > *bufferLength)
	{
		// buffer is too small, reallocate

		void* newBuffer;
		*bufferLength *= 2;
		ErrorIf(NULL == (newBuffer = context->AllocateRequestMemory(*bufferLength)), ERROR_NOT_ENOUGH_MEMORY);
		memcpy(newBuffer, *buffer, *offset);
		*buffer = newBuffer;
	}

	memcpy((char*)*buffer + *offset, content, contentLength);
	*offset += contentLength;

	return S_OK;
Error:
	return hr;
}

HRESULT CHttpProtocol::SerializeRequestHeaders(IHttpContext* context, void** result, DWORD* resultSize, DWORD* resultLength)
{
	HRESULT hr;

	CheckNull(context);
	CheckNull(result);
	CheckNull(resultSize);
	CheckNull(resultLength);

	DWORD bufferLength = CModuleConfiguration::GetInitialRequestBufferSize();
	DWORD offset = 0;
	IHttpRequest* request = context->GetRequest();
	HTTP_REQUEST* raw = request->GetRawHttpRequest();
	USHORT major, minor;
	char tmp[256];
	
	ErrorIf(NULL == (*result = context->AllocateRequestMemory(bufferLength)), ERROR_NOT_ENOUGH_MEMORY);	
	
	// Request-Line

	CheckError(CHttpProtocol::Append(context, request->GetHttpMethod(), -1, result, &bufferLength, &offset));
	CheckError(CHttpProtocol::Append(context, " ", 1, result, &bufferLength, &offset));
	CheckError(CHttpProtocol::Append(context, raw->pRawUrl, raw->RawUrlLength, result, &bufferLength, &offset));
	request->GetHttpVersion(&major, &minor);
	sprintf(tmp, " HTTP/%d.%d\n\r", major, minor);
	CheckError(CHttpProtocol::Append(context, tmp, -1, result, &bufferLength, &offset));

	// Known headers

	for (int i = 0; i < HttpHeaderRequestMaximum; i++)
	{
		if (raw->Headers.KnownHeaders[i].RawValueLength > 0)
		{
			CheckError(CHttpProtocol::Append(context, CHttpProtocol::httpRequestHeaders[i], -1, result, &bufferLength, &offset));
			CheckError(CHttpProtocol::Append(context, ": ", 2, result, &bufferLength, &offset));
			CheckError(CHttpProtocol::Append(context, raw->Headers.KnownHeaders[i].pRawValue, raw->Headers.KnownHeaders[i].RawValueLength, result, &bufferLength, &offset));
			CheckError(CHttpProtocol::Append(context, "\n\r", 2, result, &bufferLength, &offset));
		}
	}

	// Unknown headers

	for (int i = 0; i < raw->Headers.UnknownHeaderCount; i++)
	{
		CheckError(CHttpProtocol::Append(context, raw->Headers.pUnknownHeaders[i].pName, raw->Headers.pUnknownHeaders[i].NameLength, result, &bufferLength, &offset));
		CheckError(CHttpProtocol::Append(context, ": ", 2, result, &bufferLength, &offset));
		CheckError(CHttpProtocol::Append(context, raw->Headers.pUnknownHeaders[i].pRawValue, raw->Headers.pUnknownHeaders[i].RawValueLength, result, &bufferLength, &offset));
		CheckError(CHttpProtocol::Append(context, "\n\r", 2, result, &bufferLength, &offset));		
	}

	// CRLF

	CheckError(CHttpProtocol::Append(context, "\n\r", 2, result, &bufferLength, &offset));

	*resultSize = bufferLength;
	*resultLength = offset;

	return S_OK;
Error:
	
	if (NULL != result)
	{
		*result = NULL;
	}
	if (NULL != resultLength)
	{
		*resultLength = 0;
	}
	if (NULL != resultSize)
	{
		*resultSize = 0;
	}

	return hr;
}