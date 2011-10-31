#include "precomp.h"

HRESULT CNodeDebugger::GetDebugCommand(IHttpContext* context, CNodeEventProvider* log, NodeDebugCommand* debugCommand)
{
	HRESULT hr;
	DWORD physicalPathLength;
	PCWSTR physicalPath;
	DWORD scriptTranslatedLength;
	PCWSTR scriptTranslated;
	DWORD debuggerPathSegmentLength;

	scriptTranslated = context->GetScriptTranslated(&scriptTranslatedLength);
	physicalPath = context->GetPhysicalPath(&physicalPathLength);
	debuggerPathSegmentLength = CModuleConfiguration::GetDebuggerPathSegmentLength(context);

	ErrorIf(0 == physicalPathLength, ERROR_INVALID_PARAMETER);
	ErrorIf(0 == scriptTranslatedLength, ERROR_INVALID_PARAMETER);	
	ErrorIf(0 == debuggerPathSegmentLength, ERROR_INVALID_PARAMETER);

	*debugCommand = ND_NONE;

	if ((physicalPathLength - scriptTranslatedLength) <= debuggerPathSegmentLength)
	{
		return S_OK;
	}
	else if (physicalPath[scriptTranslatedLength] != L'\\')
	{
		return S_OK;
	}
	else if (0 != wcsnicmp(physicalPath + scriptTranslatedLength + 1, CModuleConfiguration::GetDebuggerPathSegment(context), debuggerPathSegmentLength))
	{
		return S_OK;
	}
	else 
	{
		WCHAR postDebugSegment = physicalPath[scriptTranslatedLength + debuggerPathSegmentLength + 1];
		if (L'\0' == postDebugSegment)
		{
			*debugCommand = ND_REDIRECT;
			return S_OK;
		}
		else if (L'\\' != postDebugSegment)
		{
			return S_OK;
		}
		else
		{
			*debugCommand = ND_DEBUG;

			HTTP_REQUEST* request = context->GetRequest()->GetRawHttpRequest();
			if (request->CookedUrl.pQueryString)
			{
				if (0 == wcsicmp(request->CookedUrl.pQueryString, L"?brk"))
				{
					*debugCommand = ND_DEBUG_BRK;
				}
				else if (0 == wcsicmp(request->CookedUrl.pQueryString, L"?kill"))
				{
					*debugCommand = ND_KILL;
				}
				else if (0 != wcsicmp(request->CookedUrl.pQueryString, L"?debug")
					&& 0 != wcsncmp(physicalPath + scriptTranslatedLength + debuggerPathSegmentLength + 1, L"\\socket.io\\", 11))
				{
					// unsupported debugger command 

					log->Log(L"iisnode received an unrecognized debugger command", WINEVENT_LEVEL_ERROR);

					// TODO, tjanczuk, return a descriptive error to the browser client
					CheckError(IISNODE_ERROR_UNRECOGNIZED_DEBUG_COMMAND);
				}
			}		
		}
	}

	return S_OK;
Error:
	return hr;
}

HRESULT CNodeDebugger::DispatchDebuggingRequest(CNodeHttpStoredContext* ctx, BOOL* requireChildContext)
{
	HRESULT hr = S_OK;
	PCWSTR actualPath;

	CheckNull(ctx);
	CheckNull(requireChildContext);

	IHttpContext* context = ctx->GetHttpContext();
	IHttpRequest* request = context->GetRequest();
	HTTP_REQUEST* rawRequest = request->GetRawHttpRequest();

	// Debugger requests for static content are to be dispatched to a static file handler via a child IIS context.
	// Debugger requests for the debugging protocol will be processed by the node-inspector app.
	// All requests in general need re-writing:
	//
	// /foo/app.js/debug/socket.io/...?abc=def -> /socket.io/...?abc=def
	// /foo/app.js/debug/bar/baz.jpg           -> /foo/app.js.debug/node_modules/node-inspector/front-end/bar/baz.jpg
	
	// Determine the minimal absolute url path that maps to the same physical path as the translated script; 
	// that minimal absolute url path will be the prefix of the full rewritten url path.

	// determine application name from script translated, e.g. 'app.js'

	DWORD scriptTranslatedLength;
	PCWSTR scriptTranslated = context->GetScriptTranslated(&scriptTranslatedLength);
	PCWSTR appName = scriptTranslated + scriptTranslatedLength - 1;
	while (appName > scriptTranslated && *appName != L'\\')
		appName--;
	appName++;
	ErrorIf(appName == scriptTranslated, ERROR_INVALID_PARAMETER);

	// determine the url fragment preceding the part of the URL to be rewritten, e.g. 'app.js/debug'

	PWSTR newPath;
	// allocate enough memory to hold all cases of the rewritten URL
	ErrorIf(NULL == (newPath = (PWSTR)context->AllocateRequestMemory(rawRequest->CookedUrl.AbsPathLength + rawRequest->CookedUrl.QueryStringLength + sizeof WCHAR * 64)), ERROR_NOT_ENOUGH_MEMORY);
	wsprintfW(newPath, L"/%s/%s", appName, CModuleConfiguration::GetDebuggerPathSegment(context));

	// determine the prefix of the absolute path that ends with the url fragment preceeding the part of the URL to be rewritten, e.g. '/foo/bar/app.js/debug'

	DWORD urlFragmentLength = wcslen(newPath);
	int indexFound = -1, currentIndex = 0;
	while (currentIndex <= ((rawRequest->CookedUrl.AbsPathLength >> 1) - urlFragmentLength))
	{
		if (0 == wcsnicmp(rawRequest->CookedUrl.pAbsPath + currentIndex, newPath, urlFragmentLength))
		{
			indexFound = currentIndex;
			break;
		}
		currentIndex++;
	}
	ErrorIf(-1 == indexFound, ERROR_INVALID_PARAMETER);

	// construct the re-written URL

	if (10 < ((rawRequest->CookedUrl.AbsPathLength >> 1) - indexFound - urlFragmentLength) 
		&& 0 == wcsnicmp(rawRequest->CookedUrl.pAbsPath + indexFound + urlFragmentLength, L"/socket.io", 10))
	{
		// this is a debugger request
		wcscpy(newPath, rawRequest->CookedUrl.pAbsPath + indexFound + urlFragmentLength); // this includes query string if present
		*requireChildContext = FALSE;
	}
	else
	{
		// this is static content request

		wcscpy(newPath, rawRequest->CookedUrl.pAbsPath);
		// replace the segment separator in 'app.js/debug' with a dot to get to 'app.js.debug'
		newPath[indexFound + wcslen(appName) + 1] = L'.'; 
		// inject the '/node_modules/node-inspector/front-end' component
		wcscpy(newPath + indexFound + urlFragmentLength, L"/node_modules/node-inspector/front-end/");
		// append the rest of the relative path (including query string if present)
		wcscpy(newPath + indexFound + urlFragmentLength + 39, rawRequest->CookedUrl.pAbsPath + indexFound + urlFragmentLength + 1);
		*requireChildContext = TRUE;
	}

	// convert to PSTR

	PSTR path;
	int pathSizeA;
	int pathSizeW = wcslen(newPath);
	ErrorIf(0 == (pathSizeA = WideCharToMultiByte(CP_ACP, 0, newPath, pathSizeW, NULL, 0, NULL, NULL)), E_FAIL);
	ErrorIf(NULL == (path = (TCHAR*)context->AllocateRequestMemory(pathSizeA + 1)), ERROR_NOT_ENOUGH_MEMORY);
	ErrorIf(pathSizeA != WideCharToMultiByte(CP_ACP, 0, newPath, pathSizeW, path, pathSizeA, NULL, NULL), E_FAIL);
	path[pathSizeA] = 0;
	ctx->SetTargetUrl(path, pathSizeA);

	return S_OK;
Error:
	return hr;
}

