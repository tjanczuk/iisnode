#include "precomp.h"

//CR: use my own heap buffer as opposed to AllocRequestMemory (IIS max buffer is 6KB anyway)

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

	if (contentLength == 0)
	{
		contentLength = strlen(content);
	}

	if ((contentLength + *offset) > *bufferLength)
	{
		DWORD quota = CModuleConfiguration::GetMaxRequestBufferSize(context);
		ErrorIf(*bufferLength >= quota, ERROR_NOT_ENOUGH_QUOTA);

		// buffer is too small, reallocate

		void* newBuffer;
		DWORD newBufferLength = *bufferLength * 2;
		if (newBufferLength > quota)
		{
			newBufferLength = quota;
		}

		ErrorIf(NULL == (newBuffer = context->AllocateRequestMemory(newBufferLength)), ERROR_NOT_ENOUGH_MEMORY);
		memcpy(newBuffer, *buffer, *offset);
		*buffer = newBuffer;
		*bufferLength = newBufferLength;
	}

	memcpy((char*)*buffer + *offset, content, contentLength);
	*offset += contentLength;

	return S_OK;
Error:
	return hr;
}

HRESULT CHttpProtocol::SerializeRequestHeaders(CNodeHttpStoredContext* ctx, void** result, DWORD* resultSize, DWORD* resultLength)
{
	HRESULT hr;
	PCSTR originalUrl = NULL;
	USHORT originalUrlLength;	

	CheckNull(ctx);
	CheckNull(result);
	CheckNull(resultSize);
	CheckNull(resultLength);

	IHttpContext* context = ctx->GetHttpContext();

	DWORD bufferLength = CModuleConfiguration::GetInitialRequestBufferSize(context);
	DWORD offset = 0;
	IHttpRequest* request = context->GetRequest();
	HTTP_REQUEST* raw = request->GetRawHttpRequest();
	USHORT major, minor;
	char tmp[256];
	
	ErrorIf(NULL == (*result = context->AllocateRequestMemory(bufferLength)), ERROR_NOT_ENOUGH_MEMORY);	
	
	// Request-Line

	CheckError(CHttpProtocol::Append(context, request->GetHttpMethod(), 0, result, &bufferLength, &offset));
	CheckError(CHttpProtocol::Append(context, " ", 1, result, &bufferLength, &offset));
	CheckError(CHttpProtocol::Append(context, ctx->GetTargetUrl(), ctx->GetTargetUrlLength(), result, &bufferLength, &offset));
	request->GetHttpVersion(&major, &minor);
	sprintf(tmp, " HTTP/%d.%d\r\n", major, minor);
	CheckError(CHttpProtocol::Append(context, tmp, 0, result, &bufferLength, &offset));

	// Known headers

	for (int i = 0; i < HttpHeaderRequestMaximum; i++)
	{
		if (raw->Headers.KnownHeaders[i].RawValueLength > 0)
		{
			CheckError(CHttpProtocol::Append(context, CHttpProtocol::httpRequestHeaders[i], 0, result, &bufferLength, &offset));
			CheckError(CHttpProtocol::Append(context, ": ", 2, result, &bufferLength, &offset));
			CheckError(CHttpProtocol::Append(context, raw->Headers.KnownHeaders[i].pRawValue, raw->Headers.KnownHeaders[i].RawValueLength, result, &bufferLength, &offset));
			CheckError(CHttpProtocol::Append(context, "\r\n", 2, result, &bufferLength, &offset));
		}
	}

	// Unknown headers

	for (int i = 0; i < raw->Headers.UnknownHeaderCount; i++)
	{
		CheckError(CHttpProtocol::Append(context, raw->Headers.pUnknownHeaders[i].pName, raw->Headers.pUnknownHeaders[i].NameLength, result, &bufferLength, &offset));
		CheckError(CHttpProtocol::Append(context, ": ", 2, result, &bufferLength, &offset));
		CheckError(CHttpProtocol::Append(context, raw->Headers.pUnknownHeaders[i].pRawValue, raw->Headers.pUnknownHeaders[i].RawValueLength, result, &bufferLength, &offset));
		CheckError(CHttpProtocol::Append(context, "\r\n", 2, result, &bufferLength, &offset));		
	}

	// CRLF

	CheckError(CHttpProtocol::Append(context, "\r\n", 2, result, &bufferLength, &offset));

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

HRESULT CHttpProtocol::ParseResponseStatusLine(CNodeHttpStoredContext* context)
{
	HRESULT hr;

	char* data = (char*)context->GetBuffer() + context->GetParsingOffset();
	DWORD dataSize = context->GetDataSize() - context->GetParsingOffset();
	DWORD offset = 0;	
	USHORT major, minor;
	DWORD count, newOffset;
	char tmp[256];
	USHORT statusCode, subStatusCode = 0;

	// HTTP-Version SP

	context->GetHttpContext()->GetRequest()->GetHttpVersion(&major, &minor);
	sprintf(tmp, "HTTP/%d.%d ", major, minor);
	count = strlen(tmp);
	ErrorIf(count >= dataSize, ERROR_MORE_DATA);
	ErrorIf(0 != memcmp(tmp, data, count), ERROR_BAD_FORMAT);
	offset += count;

	// Status-Code[.Sub-Status-Code] SP

	statusCode = 0;
	while (offset < dataSize && data[offset] >= '0' && data[offset] <= '9')
	{
		statusCode = statusCode * 10 + data[offset++] - '0';
	}
	ErrorIf(offset == dataSize, ERROR_MORE_DATA);

	if ('.' == data[offset])
	{
		// Sub-Status-Code

		offset++;		

		while (offset < dataSize && data[offset] >= '0' && data[offset] <= '9')
		{
			subStatusCode = subStatusCode * 10 + data[offset++] - '0';
		}
		ErrorIf(offset == dataSize, ERROR_MORE_DATA);
	}

	ErrorIf(' ' != data[offset], ERROR_BAD_FORMAT);
	offset++;

	// Reason-Phrase CRLF

	newOffset = offset;
	while (newOffset < (dataSize - 1) && data[newOffset] != 0x0D)
	{
		newOffset++;
	}
	ErrorIf(newOffset == dataSize - 1, ERROR_MORE_DATA);
	ErrorIf(0x0A != data[newOffset + 1], ERROR_BAD_FORMAT);
	
	// set HTTP response status line

	data[newOffset] = 0; // zero-terminate the reason phrase to reuse it without copying

	IHttpResponse* response = context->GetHttpContext()->GetResponse();
	response->Clear();
	response->SetStatus(statusCode, data + offset, subStatusCode);
	
	// adjust buffers

	context->SetParsingOffset(context->GetParsingOffset() + newOffset + 2);

	return S_OK;
Error:

	if (ERROR_MORE_DATA != hr)
	{
		context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(
			L"iisnode failed to parse response status line", WINEVENT_LEVEL_ERROR, context->GetActivityId());
	}

	return hr;
}

HRESULT CHttpProtocol::ParseResponseHeaders(CNodeHttpStoredContext* context)
{
	HRESULT hr;
	char* data = (char*)context->GetBuffer() + context->GetParsingOffset();
	DWORD dataSize = context->GetDataSize() - context->GetParsingOffset();
	DWORD offset = 0;	
	DWORD nameEndOffset, valueEndOffset;
	IHttpResponse* response = context->GetHttpContext()->GetResponse();

	while (offset < (dataSize - 1) && data[offset] != 0x0D)
	{
		// header name

		nameEndOffset = offset;
		while (nameEndOffset < dataSize && data[nameEndOffset] != ':')
		{
			nameEndOffset++;
		}
		ErrorIf(nameEndOffset == dataSize, ERROR_MORE_DATA);

		// header value

		valueEndOffset = nameEndOffset + 1;
		while (valueEndOffset < (dataSize - 1) && data[valueEndOffset] != 0x0D)
		{
			valueEndOffset++;
		}
		ErrorIf(valueEndOffset >= dataSize - 1, ERROR_MORE_DATA);
		ErrorIf(0x0A != data[valueEndOffset + 1], ERROR_BAD_FORMAT);

		// set header on response
		
		data[nameEndOffset] = 0; // zero-terminate name to reuse without copying		

		// skip the connection header because it relates to the iisnode <-> node.exe communication over named pipes 
		if (0 != strcmpi("Connection", data + offset))
		{
			data[valueEndOffset] = 0; // zero-terminate header value because this is what IHttpResponse::SetHeader expects

			// skip over ':'
			nameEndOffset++; 

			// skip over leading whitespace in value		
			while (*(data + nameEndOffset) == ' ') // data is already zero-terminated, so this loop has sentinel value
				nameEndOffset++;

			CheckError(response->SetHeader(data + offset, data + nameEndOffset, valueEndOffset - nameEndOffset, TRUE));
		}

		// adjust offsets
		
		context->SetParsingOffset(context->GetParsingOffset() + valueEndOffset - offset + 2);
		offset = valueEndOffset + 2;
	}
	ErrorIf(offset >= dataSize - 1, ERROR_MORE_DATA);
	ErrorIf(0x0A != data[offset + 1], ERROR_BAD_FORMAT);

	context->SetParsingOffset(context->GetParsingOffset() + 2);

	return S_OK;
Error:
	return hr;
}