#include "precomp.h"

HRESULT CNodeDebugger::GetDebugCommand(IHttpContext* context, CNodeEventProvider* log, NodeDebugCommand* debugCommand)
{
    HRESULT hr;
    DWORD physicalPathLength;
    PCWSTR physicalPath;
    DWORD scriptTranslatedLength;
    PCWSTR scriptTranslated;
    DWORD debuggerPathSegmentLength;

    *debugCommand = ND_NONE;

    if (!CModuleConfiguration::GetDebuggingEnabled(context))
    {
        return S_OK;
    }

    scriptTranslated = context->GetScriptTranslated(&scriptTranslatedLength);
    physicalPath = context->GetPhysicalPath(&physicalPathLength);
    debuggerPathSegmentLength = CModuleConfiguration::GetDebuggerPathSegmentLength(context);

    ErrorIf(0 == physicalPathLength, ERROR_INVALID_PARAMETER);
    ErrorIf(0 == scriptTranslatedLength, ERROR_INVALID_PARAMETER);	
    ErrorIf(0 == debuggerPathSegmentLength, ERROR_INVALID_PARAMETER);	

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
            }
        }
    }

    return S_OK;
Error:
    return hr;
}

HRESULT CNodeDebugger::DispatchDebuggingRequest(CNodeHttpStoredContext* ctx, BOOL* requireChildContext, BOOL* mainDebuggerPage)
{
    HRESULT hr = S_OK;
    PCWSTR actualPath;

    CheckNull(ctx);
    CheckNull(requireChildContext);
    CheckNull(mainDebuggerPage);

    IHttpContext* context = ctx->GetHttpContext();
    IHttpRequest* request = context->GetRequest();
    HTTP_REQUEST* rawRequest = request->GetRawHttpRequest();
    BOOL debuggerRequest = FALSE;

    // Debugger requests for static content are to be dispatched to a static file handler via a child IIS context.
    // Debugger requests for the debugging protocol will be processed by the node-inspector app.
    // All requests in general need re-writing:
    //
    // /foo/app.js/debug/socket.io/...?abc=def -> /socket.io/...?abc=def
    // /foo/app.js/debug/bar/baz.jpg           -> /foo/app.js.debug/node_modules/node-inspector/front-end/bar/baz.jpg

    // if debuggerVirtualDir is specified, request will be rewritten to reflect the actual location of the debugger files 
    // which will be under VDirPhysicalPath\SHA256(scriptPath)\app.js.debug
    // /foo/app.js/debug/bar/baz.jpg   -> /VDir/SHA256(app.js physical path)/app.js.debug/node_modules/node-inspector/front-end/bar/baz.jpg
    
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

    // determine if the request is for the main debugger page

    *mainDebuggerPage = rawRequest->CookedUrl.AbsPathLength == sizeof WCHAR * (indexFound + urlFragmentLength + 1);

    // construct the re-written URL
    if (10 < ((rawRequest->CookedUrl.AbsPathLength >> 1) - indexFound - urlFragmentLength) 
        && 0 == wcsnicmp(rawRequest->CookedUrl.pAbsPath + indexFound + urlFragmentLength, L"/socket.io", 10))
    {
        // this is a debugger request
        wcscpy(newPath, rawRequest->CookedUrl.pAbsPath + indexFound + urlFragmentLength); // this includes query string if present
        *requireChildContext = FALSE;
        debuggerRequest = TRUE;
    }
    else
    if (2 < ((rawRequest->CookedUrl.AbsPathLength >> 1) - indexFound - urlFragmentLength) 
        && 0 == wcsnicmp(rawRequest->CookedUrl.pAbsPath + indexFound + urlFragmentLength, L"/ws", 3))
    {
        // this is a debugger websocket request.
        // the request comes in for http://site/server.js/debug/ws? which needs to translated into http://site/ws?port=debugPort 
        wcscpy(newPath, rawRequest->CookedUrl.pAbsPath + indexFound + urlFragmentLength);
        WCHAR debugPortStr[10];
        _itow(ctx->GetNodeApplication()->GetDebugPort(), (PWSTR)debugPortStr, 10);
        wcscat(newPath, L"port=");
        wcscat(newPath, debugPortStr);
        *requireChildContext = FALSE;
        debuggerRequest = TRUE;
    }
    else
    if (17  < ((rawRequest->CookedUrl.AbsPathLength >> 1) - indexFound - urlFragmentLength) 
        && 0 == wcsnicmp(rawRequest->CookedUrl.pAbsPath + indexFound + urlFragmentLength, L"/node/Overrides.js", 18)
        && wcsnicmp(CModuleConfiguration::GetDebuggerExtensionDll(context), L"iisnode-inspector-", 18) == 0)
    {
        // this only applies to newer versions of node-inspector.
        // this is static content request, special case for Overrides.js because its location was modified in node-inspector.
        wcscpy(newPath, rawRequest->CookedUrl.pAbsPath);
        // replace the segment separator in 'app.js/debug' with a dot to get to 'app.js.debug'
        newPath[indexFound + wcslen(appName) + 1] = L'.'; 
        wcscpy(newPath + indexFound + urlFragmentLength, L"/node_modules/node-inspector/front-end-node/Overrides.js");
        *requireChildContext = TRUE;
    }
    else
    {
        // this is static content request
        wcscpy(newPath, rawRequest->CookedUrl.pAbsPath);
        // replace the segment separator in 'app.js/debug' with a dot to get to 'app.js.debug'
        newPath[indexFound + wcslen(appName) + 1] = L'.'; 
        // inject the '/node_modules/node-inspector/front-end' component
        wcscpy(newPath + indexFound + urlFragmentLength, L"/node_modules/node-inspector/front-end/");
        if (*mainDebuggerPage)
        {
            // requests for the main debugger page must explicitly specity the HTML file name without
            // relying on IIS'es default document handler since we must be able to set 
            // Cache-Control: no-cache header on that response and the default document handler 
            // prevents it
            if(wcsnicmp(CModuleConfiguration::GetDebuggerExtensionDll(context), L"iisnode-inspector-", 18) == 0 )
            {
                // newer versions of node-inspector have inspector.html as the starting page instead of index.html.
                wcscpy(newPath + indexFound + urlFragmentLength + 39, L"inspector.html?port=");
                WCHAR debugPortStr[10];
                _itow(ctx->GetNodeApplication()->GetDebugPort(), (PWSTR)debugPortStr, 10);
                wcscat(newPath, debugPortStr);
            }
            else
            {
                wcscpy(newPath + indexFound + urlFragmentLength + 39, L"index.html");
            }
        }
        else
        {
            // append the rest of the relative path (including query string if present)
            wcscpy(newPath + indexFound + urlFragmentLength + 39, rawRequest->CookedUrl.pAbsPath + indexFound + urlFragmentLength + 1);
        }

        *requireChildContext = TRUE;
    }

    // convert to PSTR

    LPWSTR vDir = CModuleConfiguration::GetDebuggerVirtualDir(context);
    DWORD vDirLength = CModuleConfiguration::GetDebuggerVirtualDirLength(context);

    PSTR path;
    int pathSizeA;
    LPWSTR finalPath = NULL;

    if(debuggerRequest == FALSE && vDirLength > 0)
    {
        finalPath = new WCHAR[vDirLength + CModuleConfiguration::GetDebuggerFilesPathSegmentLength(context) + wcslen(newPath) + 10];
        ErrorIf(NULL == finalPath, E_OUTOFMEMORY);
        newPath = wcsstr(newPath, appName);
        wsprintfW(finalPath, L"/%s/%s/%s", vDir, CModuleConfiguration::GetDebuggerFilesPathSegment(context), newPath);

        int pathSizeW = wcslen(finalPath);

        ErrorIf(0 == (pathSizeA = WideCharToMultiByte(CP_ACP, 0, finalPath, pathSizeW, NULL, 0, NULL, NULL)), E_FAIL);
        ErrorIf(NULL == (path = (TCHAR*)context->AllocateRequestMemory(pathSizeA + 1)), E_OUTOFMEMORY);
        ErrorIf(pathSizeA != WideCharToMultiByte(CP_ACP, 0, finalPath, pathSizeW, path, pathSizeA, NULL, NULL), E_FAIL);
        path[pathSizeA] = 0;
    }
    else
    {
        int pathSizeW = wcslen(newPath);
        ErrorIf(0 == (pathSizeA = WideCharToMultiByte(CP_ACP, 0, newPath, pathSizeW, NULL, 0, NULL, NULL)), E_FAIL);
        ErrorIf(NULL == (path = (TCHAR*)context->AllocateRequestMemory(pathSizeA + 1)), E_OUTOFMEMORY);
        ErrorIf(pathSizeA != WideCharToMultiByte(CP_ACP, 0, newPath, pathSizeW, path, pathSizeA, NULL, NULL), E_FAIL);
        path[pathSizeA] = 0;
    }

    ctx->SetTargetUrl(path, pathSizeA);

    hr = S_OK; // go do cleanup
Error:

    if( finalPath != NULL )
    {
        delete[] finalPath;
        finalPath = NULL;
    }

    return hr;
}

