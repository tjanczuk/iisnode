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

const PCSTR CHttpProtocol::schemeHttp = "http";
const PCSTR CHttpProtocol::schemeHttps = "https";

HRESULT CHttpProtocol::Append(IHttpContext* context, const char* content, DWORD contentLength, void** buffer, DWORD* bufferLength, DWORD* offset)
{
	HRESULT hr;

	if (contentLength == 0 && NULL != content)
	{
		contentLength = strlen(content);
	}

	while ((contentLength + *offset) > *bufferLength)
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

	if (contentLength > 0)
	{
		memcpy((char*)*buffer + *offset, content, contentLength);
		*offset += contentLength;
	}

	return S_OK;
Error:
	return hr;
}

HRESULT CHttpProtocol::SerializeRequestHeaders(CNodeHttpStoredContext* ctx, void** result, DWORD* resultSize, DWORD* resultLength)
{
	HRESULT hr;
	PCSTR originalUrl = NULL;
	USHORT originalUrlLength;
	DWORD remoteHostSize = INET6_ADDRSTRLEN + 1;
	char remoteHost[INET6_ADDRSTRLEN + 1];
	BOOL addXFF, addXFP;
	char** serverVars;
	int serverVarCount;

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
	const int tmpSize = 256;
	char tmp[tmpSize];
	PCSTR method = request->GetHttpMethod();
	
	ErrorIf(NULL == (*result = context->AllocateRequestMemory(bufferLength)), ERROR_NOT_ENOUGH_MEMORY);	

	// Determine whether response entity body is to be expected

	if (method && 0 == strcmpi("HEAD", method))
	{
		// HEAD requests do not have response entity body
		// http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.4

		ctx->SetExpectResponseBody(FALSE);
	}
	
	// Request-Line

	CheckError(CHttpProtocol::Append(context, method, 0, result, &bufferLength, &offset));
	CheckError(CHttpProtocol::Append(context, " ", 1, result, &bufferLength, &offset));
	CheckError(CHttpProtocol::Append(context, ctx->GetTargetUrl(), ctx->GetTargetUrlLength(), result, &bufferLength, &offset));
	request->GetHttpVersion(&major, &minor);
	if (1 == major && 1 == minor)
	{
		CheckError(CHttpProtocol::Append(context, " HTTP/1.1\r\n", 11, result, &bufferLength, &offset));
	}
	else if (1 == major && 0 == minor)
	{
		CheckError(CHttpProtocol::Append(context, " HTTP/1.0\r\n", 11, result, &bufferLength, &offset));
	}
	else
	{
		sprintf(tmp, " HTTP/%d.%d\r\n", major, minor);
		CheckError(CHttpProtocol::Append(context, tmp, 0, result, &bufferLength, &offset));
	}

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

	if (TRUE == (addXFF = addXFP = CModuleConfiguration::GetEnableXFF(context)))
	{
		PSOCKADDR addr = request->GetRemoteAddress();
		DWORD addrSize = addr->sa_family == AF_INET ? sizeof SOCKADDR_IN : sizeof SOCKADDR_IN6;
		ErrorIf(0 != GetNameInfo(addr, addrSize, remoteHost, remoteHostSize, NULL, 0, NI_NUMERICHOST), GetLastError());

		// Set remoteHostSize to the size of remoteHost
		remoteHostSize = strlen(remoteHost);
	}

	for (int i = 0; i < raw->Headers.UnknownHeaderCount; i++)
	{
		CheckError(CHttpProtocol::Append(context, raw->Headers.pUnknownHeaders[i].pName, raw->Headers.pUnknownHeaders[i].NameLength, result, &bufferLength, &offset));
		CheckError(CHttpProtocol::Append(context, ": ", 2, result, &bufferLength, &offset));
		CheckError(CHttpProtocol::Append(context, raw->Headers.pUnknownHeaders[i].pRawValue, raw->Headers.pUnknownHeaders[i].RawValueLength, result, &bufferLength, &offset));

		if (addXFF && 15 == raw->Headers.pUnknownHeaders[i].NameLength && 0 == _stricmp("X-Forwarded-For", raw->Headers.pUnknownHeaders[i].pName))
		{
			// augment existing X-Forwarded-For header, but only if the last item isn't the same IP as what we are adding
			// (fixes https://github.com/tjanczuk/iisnode/issues/340)			 
			if (raw->Headers.pUnknownHeaders[i].RawValueLength < remoteHostSize ||
				0 != _stricmp(remoteHost, (raw->Headers.pUnknownHeaders[i].pRawValue + (raw->Headers.pUnknownHeaders[i].RawValueLength - remoteHostSize))))
			{
				CheckError(CHttpProtocol::Append(context, ", ", 2, result, &bufferLength, &offset));
				CheckError(CHttpProtocol::Append(context, remoteHost, 0, result, &bufferLength, &offset));
			}

			addXFF = FALSE;
		}
		else if (addXFP && 17 == raw->Headers.pUnknownHeaders[i].NameLength && 0 == _stricmp("X-Forwarded-Proto", raw->Headers.pUnknownHeaders[i].pName))
		{
			// Already exists in incoming headers; don't add a second one
			addXFP = FALSE;
		}

		CheckError(CHttpProtocol::Append(context, "\r\n", 2, result, &bufferLength, &offset));		
	}

	if (addXFF)
	{
		// add a new X-Forwarded-For header

		CheckError(CHttpProtocol::Append(context, "X-Forwarded-For: ", 17, result, &bufferLength, &offset));
		CheckError(CHttpProtocol::Append(context, remoteHost, 0, result, &bufferLength, &offset));
		CheckError(CHttpProtocol::Append(context, "\r\n", 2, result, &bufferLength, &offset));		
	}

	if (addXFP)
	{
		// Determine the incoming request protocol scheme
		PCSTR varValue, varScheme = schemeHttp;
		DWORD varValueLength;
		if (S_OK == context->GetServerVariable("HTTPS", &varValue, &varValueLength))
		{
			if (0 == _stricmp("on", varValue))
			{
				varScheme = schemeHttps;
			}
		}

		// Add the X-Forwarded-Proto header (default is http)
		CheckError(CHttpProtocol::Append(context, "X-Forwarded-Proto: ", 19, result, &bufferLength, &offset));
		CheckError(CHttpProtocol::Append(context, varScheme, 0, result, &bufferLength, &offset));
		CheckError(CHttpProtocol::Append(context, "\r\n", 2, result, &bufferLength, &offset));	
	}

	// promote server variables

	CheckError(CModuleConfiguration::GetPromoteServerVars(context, &serverVars, &serverVarCount));
	while (serverVarCount)
	{
		serverVarCount--;
		PCSTR varValue;
		DWORD varValueLength;
		if (S_OK == context->GetServerVariable(serverVars[serverVarCount], &varValue, &varValueLength))
		{
			CheckError(CHttpProtocol::Append(context, "X-iisnode-", 10, result, &bufferLength, &offset));
			CheckError(CHttpProtocol::Append(context, serverVars[serverVarCount], 0, result, &bufferLength, &offset));
			CheckError(CHttpProtocol::Append(context, ": ", 2, result, &bufferLength, &offset));
			CheckError(CHttpProtocol::Append(context, varValue, varValueLength, result, &bufferLength, &offset));
			CheckError(CHttpProtocol::Append(context, "\r\n", 2, result, &bufferLength, &offset));		
		}
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
	char* tmp1;
	USHORT statusCode, subStatusCode = 0;

	// HTTP-Version SP

	context->GetHttpContext()->GetRequest()->GetHttpVersion(&major, &minor);
	if (1 == major && 1 == minor)
	{
		tmp1 = "HTTP/1.1 ";
		count = 9;
	}
	else if (1 == major && 0 == minor)
	{
		tmp1 = "HTTP/1.0 ";
		count = 9;
	}
	else
	{
		sprintf(tmp, "HTTP/%d.%d ", major, minor);
		count = strlen(tmp);
		tmp1 = tmp;
	}

	ErrorIf(count >= dataSize, ERROR_MORE_DATA);
	ErrorIf(0 != memcmp(tmp1, data, 5), ERROR_BAD_FORMAT);
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

	// Determine whether to expect response entity body
	// http://www.w3.org/Protocols/rfc2616/rfc2616-sec4.html#sec4.4

	if (statusCode == 101) 
	{
		CheckError(context->SetupUpgrade());
	}
	else if (statusCode >= 100 && statusCode < 200
		|| statusCode == 204
		|| statusCode == 304)
	{
		context->SetExpectResponseBody(FALSE);
	}

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
	response->SetStatus(statusCode, data + offset, subStatusCode);
	
	// adjust buffers

	context->SetParsingOffset(context->GetParsingOffset() + newOffset + 2);

	return S_OK;
Error:

	if (ERROR_MORE_DATA != hr)
	{
        context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log( context->GetHttpContext(),
			L"iisnode failed to parse response status line", WINEVENT_LEVEL_ERROR, context->GetActivityId());
	}

	return hr;
}

HRESULT CHttpProtocol::ParseChunkHeader(CNodeHttpStoredContext* context)
{
	HRESULT hr;

	char* data = (char*)context->GetBuffer() + context->GetParsingOffset();
	char* current;
	char* chunkHeaderStart;
	DWORD dataSize = context->GetDataSize() - context->GetParsingOffset();
	ULONG chunkLength = 0;
	ULONG totalChunkLength = 0;

	// attempt to parse as many response body chunks as there are buffered in memory

	current = data;
	do
	{
		// parse chunk length
		
		chunkHeaderStart = current;
		chunkLength = 0;
		while (true)
		{
			ErrorIf((current - data) >= dataSize, ERROR_MORE_DATA);
			if (*current >= 'A' && *current <= 'F')
			{
				chunkLength <<= 4;
				chunkLength += *current - 'A' + 10;
			}
			else if (*current >= 'a' && *current <= 'f')
			{
				chunkLength <<= 4;
				chunkLength += *current - 'a' + 10;
			}
			else if (*current >= '0' && *current <= '9')
			{
				chunkLength <<= 4;
				chunkLength += *current - '0';
			}
			else 
			{
				ErrorIf(current == chunkHeaderStart, ERROR_BAD_FORMAT); // no hex digits found
				break;
			}

			current++;
		}

		// skip optional extensions

		while (true)
		{
			ErrorIf((current - data) >= dataSize, ERROR_MORE_DATA);
			if (*current == 0x0D)
			{
				break;
			}

			current++;
		}

		// LF

		current++;
		ErrorIf((current - data) >= dataSize, ERROR_MORE_DATA);
		ErrorIf(*current != 0x0A, ERROR_BAD_FORMAT);
		current++;

		// remember total length of all parsed chunks before attempting to parse subsequent chunk header

		// set total chunk length to include current chunk content length, previously parsed chunks (with headers), 
		// plus the CRLF following the current chunk content
		totalChunkLength = chunkLength + (ULONG)(current - data) + 2; 
		current += chunkLength + 2; // chunk content length + CRLF

	} while (chunkLength != 0); // exit when last chunk has been detected	

	// if we are here, current buffer contains the header of the last chunk of the response

	context->SetChunkLength(totalChunkLength);
	context->SetIsLastChunk(TRUE);
	context->SetChunkTransmitted(0);

	return S_OK;

Error:

	if (ERROR_MORE_DATA != hr)
	{
        context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log( context->GetHttpContext(),
			L"iisnode failed to parse response body chunk header", WINEVENT_LEVEL_ERROR, context->GetActivityId());

		return hr;
	}
	else if (0 < totalChunkLength)
	{
		// at least one response chunk has been successfuly parsed, but more chunks remain

		context->SetChunkLength(totalChunkLength);
		context->SetIsLastChunk(FALSE);
		context->SetChunkTransmitted(0);

		return S_OK;
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
	BOOL needConnectionHeaderFixup = FALSE;

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

		// Skip the Connection header because it relates to the iisnode <-> node.exe communication over named pipes,
		// unless this is a 101 response to HTTP Upgrade request, in which case this is used to inform HTTP.SYS to keep the
		// connection open.
		if (context->GetIsUpgrade() || 0 != strcmpi("Connection", data + offset))
		{
			data[valueEndOffset] = 0; // zero-terminate header value because this is what IHttpResponse::SetHeader expects

			// skip over ':'
			nameEndOffset++; 

			// skip over leading whitespace in value		
			while (*(data + nameEndOffset) == ' ') // data is already zero-terminated, so this loop has sentinel value
				nameEndOffset++;

			if (context->GetIsUpgrade() && 0 == strcmpi("Connection", data + offset))
			{
				// Add the Connection header under a custom name to force it to be added to unknown headers collection.
				// Header name will subsequently be fixed up back to Connection. 
				// The end result is that the Connection header ends up in the unknown headers collection rather then
				// known headers collection where it would get ignored by http.sys.
				
				CheckError(response->SetHeader("x-iisnode-connection", data + nameEndOffset, valueEndOffset - nameEndOffset, FALSE));
				needConnectionHeaderFixup = TRUE;
			}
			else
			{
				CheckError(response->SetHeader(data + offset, data + nameEndOffset, valueEndOffset - nameEndOffset, FALSE));
			}
		}
		else if ((valueEndOffset - nameEndOffset) >= 5 && 0 == memcmp((void*)(data + valueEndOffset - 5), "close", 5))
		{
			context->SetCloseConnection(TRUE);
		}

		// adjust offsets
		
		context->SetParsingOffset(context->GetParsingOffset() + valueEndOffset - offset + 2);
		offset = valueEndOffset + 2;
	}

	ErrorIf(offset >= dataSize - 1, ERROR_MORE_DATA);
	ErrorIf(0x0A != data[offset + 1], ERROR_BAD_FORMAT);

	context->SetParsingOffset(context->GetParsingOffset() + 2);

	if (needConnectionHeaderFixup)
	{
		HTTP_RESPONSE* rawResponse = response->GetRawHttpResponse();
		for (int i = 0; i < response->GetRawHttpResponse()->Headers.UnknownHeaderCount; i++)
		{
			if (20 == rawResponse->Headers.pUnknownHeaders[i].NameLength 
				&& 0 == _strnicmp("x-iisnode-connection", rawResponse->Headers.pUnknownHeaders[i].pName, 20))
			{
				rawResponse->Headers.pUnknownHeaders[i].NameLength = 10;
				rawResponse->Headers.pUnknownHeaders[i].pName = "Connection";
				break;
			}
		}
	}

	return S_OK;
Error:
	return hr;
}