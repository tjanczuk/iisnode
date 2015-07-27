#include "precomp.h"

HRESULT CProtocolBridge::PostponeProcessing(CNodeHttpStoredContext* context, DWORD dueTime)
{
    CAsyncManager* async = context->GetNodeApplication()->GetApplicationManager()->GetAsyncManager();
    LARGE_INTEGER delay;
    delay.QuadPart = dueTime;
    delay.QuadPart *= -10000; // convert from ms to 100ns units

    return async->SetTimer(context->GetAsyncContext(), &delay);
}

#define LOCAL127            0x0100007F  // 127.0.0.1

BOOL CProtocolBridge::IsLocalCall(IHttpContext* ctx)
{
    PSOCKADDR src = ctx->GetRequest()->GetRemoteAddress();
    PSOCKADDR dest = ctx->GetRequest()->GetLocalAddress();

    if (AF_INET == src->sa_family && AF_INET == dest->sa_family)
    {
        DWORD srcAddress = ntohl(((PSOCKADDR_IN)src)->sin_addr.s_addr);
        DWORD destAddress = ntohl(((PSOCKADDR_IN)dest)->sin_addr.s_addr);

        return srcAddress == destAddress || LOCAL127 == srcAddress || LOCAL127 == destAddress;
    }
    else if (AF_INET6 == src->sa_family && AF_INET6 == dest->sa_family)
    {
        IN6_ADDR* srcAddress = &((PSOCKADDR_IN6)src)->sin6_addr;
        IN6_ADDR* destAddress = &((PSOCKADDR_IN6)dest)->sin6_addr;

        if (0 == memcmp(srcAddress, destAddress, sizeof IN6_ADDR))
        {
            return TRUE;
        }

        if (IN6_IS_ADDR_LOOPBACK(srcAddress) || IN6_IS_ADDR_LOOPBACK(destAddress))
        {
            return TRUE;
        }
    }

    return FALSE;
}

BOOL CProtocolBridge::SendIisnodeError(IHttpContext* httpCtx, HRESULT hr)
{
    if (IISNODE_ERROR_UNABLE_TO_READ_CONFIGURATION == hr || IISNODE_ERROR_UNABLE_TO_READ_CONFIGURATION_OVERRIDE == hr)
    {
        if (CProtocolBridge::IsLocalCall(httpCtx))
        {
            switch (hr) {

            default:
                return FALSE;

            case IISNODE_ERROR_UNABLE_TO_READ_CONFIGURATION:
                CProtocolBridge::SendSyncResponse(
                    httpCtx, 
                    200, 
                    "OK", 
                    hr, 
                    TRUE, 
                    "iisnode was unable to read the configuration file. Make sure the web.config file syntax is correct. In particular, verify the "
                    " <a href=""https://github.com/tjanczuk/iisnode/blob/master/src/samples/configuration/web.config"">"
                    "iisnode configuration section</a> matches the expected schema. The schema of the iisnode section that your version of iisnode requires is stored in the "
                    "%systemroot%\\system32\\inetsrv\\config\\schema\\iisnode_schema.xml file.");
                break;

            case IISNODE_ERROR_UNABLE_TO_READ_CONFIGURATION_OVERRIDE:
                CProtocolBridge::SendSyncResponse(
                    httpCtx, 
                    200, 
                    "OK", 
                    hr, 
                    TRUE, 
                    "iisnode was unable to read the configuration file iisnode.yml. Make sure the iisnode.yml file syntax is correct. For reference, check "
                    " <a href=""https://github.com/tjanczuk/iisnode/blob/master/src/samples/configuration/iisnode.yml"">"
                    "the sample iisnode.yml file</a>. The property names recognized in the iisnode.yml file of your version of iisnode are stored in the "
                    "%systemroot%\\system32\\inetsrv\\config\\schema\\iisnode_schema.xml file.");
                break;
            };

            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }

    if (!CModuleConfiguration::GetDevErrorsEnabled(httpCtx))
    {
        return FALSE;
    }

    switch (hr) {

    default: 
        return FALSE;

    case IISNODE_ERROR_UNRECOGNIZED_DEBUG_COMMAND:
        CProtocolBridge::SendSyncResponse(
            httpCtx, 
            200, 
            "OK", 
            hr, 
            TRUE, 
            "Unrecognized debugging command. Supported commands are ?debug (default), ?brk, and ?kill.");
        break;

    case IISNODE_ERROR_UNABLE_TO_FIND_DEBUGGING_PORT:
        CProtocolBridge::SendSyncResponse(
            httpCtx, 
            200, 
            "OK", 
            hr, 
            TRUE, 
            "The debugger was unable to acquire a TCP port to establish communication with the debugee. "
            "This may be due to lack of free TCP ports in the range specified in the <a href=""https://github.com/tjanczuk/iisnode/blob/master/src/samples/configuration/web.config"">"
            "system.webServer/iisnode/@debuggerPortRange</a> configuration "
            "section, or due to lack of permissions to create TCP listeners by the identity of the IIS worker process.");
        break;

    case IISNODE_ERROR_UNABLE_TO_CONNECT_TO_DEBUGEE:
        CProtocolBridge::SendSyncResponse(
            httpCtx, 
            200, 
            "OK", 
            hr, 
            TRUE, 
            "The debugger was unable to connect to the the debugee. "
            "This may be due to the debugee process terminating during startup (e.g. due to an unhandled exception) or "
            "failing to establish a TCP listener on the debugging port. ");
        break;

    case IISNODE_ERROR_INSPECTOR_NOT_FOUND:
        CProtocolBridge::SendSyncResponse(
            httpCtx, 
            200, 
            "OK", 
            hr, 
            TRUE, 
            "The version of iisnode installed on the server does not support remote debugging. "
            "To use remote debugging, please update your iisnode installation on the server to one available from "
            "<a href=""http://github.com/tjanczuk/iisnode/downloads"">http://github.com/tjanczuk/iisnode/downloads</a>. "
            "We apologize for inconvenience.");
        break;

    case IISNODE_ERROR_UNABLE_TO_CREATE_DEBUGGER_FILES:
        CProtocolBridge::SendSyncResponse(
            httpCtx, 
            200, 
            "OK", 
            hr, 
            TRUE, 
            "The iisnode module is unable to deploy supporting files necessary to initialize the debugger. "
            "Please check that the identity of the IIS application pool running the node.js application has read and write access "
            "permissions to the directory on the server where the node.js application is located.");
        break;

    case IISNODE_ERROR_UNABLE_TO_START_NODE_EXE:
        char* errorMessage = 
                "The iisnode module is unable to start the node.exe process. Make sure the node.exe executable is available "
                "at the location specified in the <a href=""https://github.com/tjanczuk/iisnode/blob/master/src/samples/configuration/web.config"">"
                "system.webServer/iisnode/@nodeProcessCommandLine</a> element of web.config. "
                "By default node.exe is expected in one of the directories listed in the PATH environment variable.";

        CProtocolBridge::SendSyncResponse(
            httpCtx, 
            200, 
            "OK", 
            hr, 
            TRUE, 
            errorMessage);
        break;

    };

    return TRUE;
}

BOOL CProtocolBridge::SendIisnodeError(CNodeHttpStoredContext* ctx, HRESULT hr)
{
    if (CProtocolBridge::SendIisnodeError(ctx->GetHttpContext(), hr))
    {
        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(),
            L"iisnode request processing failed for reasons recognized by iisnode", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

        if (INVALID_HANDLE_VALUE != ctx->GetPipe())
        {
            CloseHandle(ctx->GetPipe());
            ctx->SetPipe(INVALID_HANDLE_VALUE);
        }

        CProtocolBridge::FinalizeResponseCore(
            ctx, 
            RQ_NOTIFICATION_FINISH_REQUEST, 
            hr, 
            ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider(),
            L"iisnode posts completion from SendIisnodeError", 
            WINEVENT_LEVEL_VERBOSE);

        return TRUE;
    }

    return FALSE;
}

BOOL CProtocolBridge::SendDevError(CNodeHttpStoredContext* context, 
                                   USHORT status, 
                                   USHORT subStatus,
                                   PCTSTR reason, 
                                   HRESULT hresult, 
                                   BOOL disableCache)
{
    HRESULT hr;
    DWORD rawLogSize, htmlLogSize, htmlTotalSize;
    IHttpContext* ctx;
    char* rawLog;
    char* templ1;
    char* templ2;
    char* templ3;
    char* html;
    char* current;

    if (500 <= status && CModuleConfiguration::GetDevErrorsEnabled(context->GetHttpContext()) && NULL != context->GetNodeProcess())
    {
        // return a friendly HTTP 200 response with HTML entity body that contains error details 
        // and the content of node.exe stdout/err, if available

        ctx = context->GetHttpContext();		
        rawLog = context->GetNodeProcess()->TryGetLog(ctx, &rawLogSize);
        templ1 = 
            "<p>iisnode encountered an error when processing the request.</p><pre style=\"background-color: eeeeee\">"
            "HRESULT: 0x%x\n"
            "HTTP status: %d\n"
            "HTTP subStatus: %d\n"
            "HTTP reason: %s</pre>"
            "<p>You are receiving this HTTP 200 response because <a href=""https://github.com/tjanczuk/iisnode/blob/master/src/samples/configuration/web.config"">"
            "system.webServer/iisnode/@devErrorsEnabled</a> configuration setting is 'true'.</p>"
            "<p>In addition to the log of stdout and stderr of the node.exe process, consider using "
            "<a href=""http://tomasz.janczuk.org/2011/11/debug-nodejs-applications-on-windows.html"">debugging</a> "
            "and <a href=""http://tomasz.janczuk.org/2011/09/using-event-tracing-for-windows-to.html"">ETW traces</a> to further diagnose the problem.</p>";
        templ2 = "<p>The node.exe process has not written any information to stderr or iisnode was unable to capture this information. "
            "Frequent reason is that the iisnode module is unable to create a log file to capture stdout and stderr output from node.exe. "
            "Please check that the identity of the IIS application pool running the node.js application has read and write access "
            "permissions to the directory on the server where the node.js application is located. Alternatively you can disable logging "
            "by setting <a href=""https://github.com/tjanczuk/iisnode/blob/master/src/samples/configuration/web.config"">"
            "system.webServer/iisnode/@loggingEnabled</a> element of web.config to 'false'.";
        templ3="<p>You may get additional information about this error condition by logging stdout and stderr of the node.exe process."
            "To enable logging, set the <a href=""https://github.com/tjanczuk/iisnode/blob/master/src/samples/configuration/web.config"">"
            "system.webServer/iisnode/@loggingEnabled</a> configuration setting to 'true' (current value is 'false').</p>";

        // calculate HTML encoded size of the log

        htmlLogSize = 0;
        for (int i = 0; i < rawLogSize; i++)
        {
            if (rawLog[i] >= 0 && rawLog[i] <= 0x8
                || rawLog[i] >= 0xb && rawLog[i] <= 0xc
                || rawLog[i] >= 0xe && rawLog[i] <= 0x1f
                || rawLog[i] >= 0x80 && rawLog[i] <= 0x9f)
            {
                // characters disallowed in HTML will be converted to [xAB] notation, thus taking 5 bytes
                htmlLogSize += 5;
            }
            else
            {
                switch (rawLog[i]) 
                {
                default:
                    htmlLogSize++; 
                    break;
                case '&':
                    htmlLogSize += 5; // &amp;
                    break;
                case '<':
                case '>':
                    htmlLogSize += 4; // &lt; &gt;
                    break;
                case '"':
                case '\'':
                    htmlLogSize += 6; // &quot; &apos;
                    break;
                };
            }
        }

        // calculate total size of HTML and allocate memory

        htmlTotalSize = strlen(templ1) + 20 /* hresult */ + 20 /* status code */ + strlen(reason) + strlen(templ2) /* log content heading */ + htmlLogSize;
        ErrorIf(NULL == (html = (char*)ctx->AllocateRequestMemory(htmlTotalSize)), ERROR_NOT_ENOUGH_MEMORY);
        RtlZeroMemory(html, htmlTotalSize);

        // construct the HTML

        sprintf(html, templ1, hresult, status, subStatus, reason);

        if (0 == rawLogSize)
        {
            if (CModuleConfiguration::GetLoggingEnabled(ctx))
            {
                strcat(html, templ2);
            }
            else
            {
                strcat(html, templ3);
            }
        }
        else
        {
            strcat(html, "<p>The last 64k of the output generated by the node.exe process to stderr is shown below:</p><pre style=\"background-color: eeeeee\">");

            current = html + strlen(html);

            if (htmlLogSize == rawLogSize)
            {
                // no HTML encoding is necessary
                
                memcpy(current, rawLog, rawLogSize);
            }
            else
            {
                // HTML encode the log

                for (int i = 0; i < rawLogSize; i++)
                {
                    if (rawLog[i] >= 0 && rawLog[i] <= 0x8
                        || rawLog[i] >= 0xb && rawLog[i] <= 0xc
                        || rawLog[i] >= 0xe && rawLog[i] <= 0x1f
                        || rawLog[i] >= 0x80 && rawLog[i] <= 0x9f)
                    {
                        // characters disallowed in HTML are converted to [xAB] notation
                        *current++ = '[';
                        *current++ = 'x';
                        *current = rawLog[i] >> 4;
                        *current++ += *current > 9 ? 'A' - 10 : '0';
                        *current = rawLog[i] & 15;
                        *current++ += *current > 9 ? 'A' - 10 : '0';
                        *current++ = ']';
                    }
                    else
                    {
                        switch (rawLog[i]) 
                        {
                        default:
                            *current++ = rawLog[i];
                            break;
                        case '&':
                            *current++ = '&';
                            *current++ = 'a';
                            *current++ = 'm';
                            *current++ = 'p';
                            *current++ = ';';
                            break;
                        case '<':
                            *current++ = '&';
                            *current++ = 'l';
                            *current++ = 't';
                            *current++ = ';';
                            break;
                        case '>':
                            *current++ = '&';
                            *current++ = 'g';
                            *current++ = 't';
                            *current++ = ';';
                            break;
                        case '"':
                            *current++ = '&';
                            *current++ = 'q';
                            *current++ = 'u';
                            *current++ = 'o';
                            *current++ = 't';
                            *current++ = ';';
                            break;
                        case '\'':
                            *current++ = '&';
                            *current++ = 'a';
                            *current++ = 'p';
                            *current++ = 'o';
                            *current++ = 's';
                            *current++ = ';';
                            break;
                        };
                    }
                }
            }
        }

        // send the response

        CheckError(CProtocolBridge::SendSyncResponse(
            ctx,
            200,
            "OK",
            hresult,
            TRUE,
            html));

        return true;
    }

Error:
    return false;
}

HRESULT CProtocolBridge::SendEmptyResponse(CNodeHttpStoredContext* context, USHORT status, USHORT subStatus, PCTSTR reason, HRESULT hresult, BOOL disableCache)
{
    context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(context->GetHttpContext(),
        L"iisnode request processing failed for reasons unrecognized by iisnode", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());

    if (INVALID_HANDLE_VALUE != context->GetPipe())
    {
        CloseHandle(context->GetPipe());
        context->SetPipe(INVALID_HANDLE_VALUE);
    }

    if (!CProtocolBridge::SendDevError(context, status, subStatus, reason, hresult, disableCache))
    {
        // return default IIS response

        CProtocolBridge::SendEmptyResponse(context->GetHttpContext(), status, subStatus, reason, hresult, disableCache);
    }

    CProtocolBridge::FinalizeResponseCore(
        context, 
        RQ_NOTIFICATION_FINISH_REQUEST, 
        hresult, 
        context->GetNodeApplication()->GetApplicationManager()->GetEventProvider(),
        L"iisnode posts completion from SendEmtpyResponse", 
        WINEVENT_LEVEL_VERBOSE);

    return S_OK;
}

HRESULT CProtocolBridge::SendSyncResponse(IHttpContext* httpCtx, USHORT status, PCTSTR reason, HRESULT hresult, BOOL disableCache, PCSTR htmlBody)
{
    HRESULT hr;
    DWORD bytesSent;
    HTTP_DATA_CHUNK body;

    CProtocolBridge::SendEmptyResponse(httpCtx, status, 0, reason, hresult, disableCache);

    IHttpResponse* response = httpCtx->GetResponse();
    response->SetHeader(HttpHeaderContentType, "text/html", 9, TRUE);
    body.DataChunkType = HttpDataChunkFromMemory;
    body.FromMemory.pBuffer = (PVOID)htmlBody;
    body.FromMemory.BufferLength = strlen(htmlBody);
    CheckError(response->WriteEntityChunks(&body, 1, FALSE, FALSE, &bytesSent));

    return S_OK;
Error:
    return hr;
}

void CProtocolBridge::SendEmptyResponse(IHttpContext* httpCtx, USHORT status, USHORT subStatus, PCTSTR reason, HRESULT hresult, BOOL disableCache)
{
    if (!httpCtx->GetResponseHeadersSent())
    {
        httpCtx->GetResponse()->Clear();
        httpCtx->GetResponse()->SetStatus(status, reason, subStatus, hresult);
        if (disableCache)
        {
            httpCtx->GetResponse()->SetHeader(HttpHeaderCacheControl, "no-cache", 8, TRUE);
        }
    }
}

HRESULT CProtocolBridge::InitiateRequest(CNodeHttpStoredContext* context)
{
    HRESULT hr;
    BOOL requireChildContext = FALSE;	
    BOOL mainDebuggerPage = FALSE;
    IHttpContext* child = NULL;
    BOOL completionExpected;

    // determine what the target path of the request is

    if (context->GetNodeApplication()->IsDebugger())
    {
        // All debugger URLs require rewriting. Requests for static content will be processed using a child http context and served
        // by a static file handler. Debugging protocol requests will continue executing in the current context and be processed 
        // by the node-inspector application.

        CheckError(CNodeDebugger::DispatchDebuggingRequest(context, &requireChildContext, &mainDebuggerPage));
    }
    else
    {
        // For application requests, if the URL rewrite module had been used to rewrite the URL, 
        // present the original URL to the node.js application instead of the re-written one.

        PCSTR url;
        USHORT urlLength;
        IHttpRequest* request = context->GetHttpContext()->GetRequest();

        if (NULL == (url = request->GetHeader("X-Original-URL", &urlLength)))
        {
            HTTP_REQUEST* raw = request->GetRawHttpRequest();
            
            // Fix for https://github.com/tjanczuk/iisnode/issues/296
            PSTR path = NULL;
            int pathSizeA = 0;
            int cchAbsPathLength = (raw->CookedUrl.AbsPathLength + raw->CookedUrl.QueryStringLength) >> 1;
            ErrorIf(0 == (pathSizeA = WideCharToMultiByte(CP_ACP, 0, raw->CookedUrl.pAbsPath, cchAbsPathLength, NULL, 0, NULL, NULL)), E_FAIL);
            ErrorIf(NULL == (path = (TCHAR*)context->GetHttpContext()->AllocateRequestMemory(pathSizeA + 1)), ERROR_NOT_ENOUGH_MEMORY);
            ErrorIf(pathSizeA != WideCharToMultiByte(CP_ACP, 0, raw->CookedUrl.pAbsPath, cchAbsPathLength, path, pathSizeA, NULL, NULL), E_FAIL);
            path[pathSizeA] = 0;

            context->SetTargetUrl(path, pathSizeA);
        }
        else
        {
            context->SetTargetUrl(url, urlLength);
        }
    }

    // determine how to process the request

    if (requireChildContext)
    {
        CheckError(context->GetHttpContext()->CloneContext(CLONE_FLAG_BASICS | CLONE_FLAG_ENTITY | CLONE_FLAG_HEADERS, &child));
        if (mainDebuggerPage)
        {
            // Prevent client caching of responses to requests for debugger entry points e.g. app.js/debug or app.js/debug?brk
            // This is to allow us to run initialization logic on the server if necessary every time user refreshes the page
            // Static content subordinate to the main debugger page is eligible for client side caching

            child->GetResponse()->SetHeader(HttpHeaderCacheControl, "no-cache", 8, TRUE);
        }

        CheckError(child->GetRequest()->SetUrl(context->GetTargetUrl(), context->GetTargetUrlLength(), FALSE));
        context->SetChildContext(child);
        context->SetNextProcessor(CProtocolBridge::ChildContextCompleted);
        
        CheckError(context->GetHttpContext()->ExecuteRequest(TRUE, child, 0, NULL, &completionExpected));
        if (!completionExpected)
        {
            CProtocolBridge::ChildContextCompleted(S_OK, 0, context->GetOverlapped());
        }
    }
    else
    {
        context->SetNextProcessor(CProtocolBridge::CreateNamedPipeConnection);
        CProtocolBridge::CreateNamedPipeConnection(S_OK, 0, context->InitializeOverlapped());
    }

    return S_OK; 
Error:

    if (child)
    {		
        child->ReleaseClonedContext();
        child = NULL;
        context->SetChildContext(NULL);
    }

    return hr;
}

void WINAPI CProtocolBridge::ChildContextCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
    CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

    ctx->GetChildContext()->ReleaseClonedContext();
    ctx->SetChildContext(NULL);

    if (S_OK == error)
    {
        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(),
            L"iisnode finished processing child http request", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
    }
    else
    {
        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(),
            L"iisnode failed to process child http request", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
    }

    ctx->SetHresult(error);
    ctx->SetRequestNotificationStatus(RQ_NOTIFICATION_CONTINUE);	
    ctx->SetNextProcessor(NULL);
    
    CProtocolBridge::FinalizeResponseCore(
        ctx, 
        RQ_NOTIFICATION_CONTINUE, 
        error, 
        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider(), 
        L"iisnode posts completion from ChildContextCompleted", 
        WINEVENT_LEVEL_VERBOSE);
    
    return;
}

void WINAPI CProtocolBridge::CreateNamedPipeConnection(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
    HRESULT hr;
    CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);
    HANDLE pipe = INVALID_HANDLE_VALUE;
    DWORD retry = ctx->GetConnectionRetryCount();

    if (0 == retry)
    {
        // only the first connection attempt uses connections from the pool

        pipe = ctx->GetNodeProcess()->GetConnectionPool()->Take();
    }

    if (INVALID_HANDLE_VALUE == pipe)
    {
        ErrorIf(INVALID_HANDLE_VALUE == (pipe = CreateFile(
            ctx->GetNodeProcess()->GetNamedPipeName(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_OVERLAPPED,
            NULL)), 
            GetLastError());

        ErrorIf(!SetFileCompletionNotificationModes(
            pipe, 
            FILE_SKIP_COMPLETION_PORT_ON_SUCCESS | FILE_SKIP_SET_EVENT_ON_HANDLE), 
            GetLastError());

        ctx->SetIsConnectionFromPool(FALSE);
        ctx->GetNodeApplication()->GetApplicationManager()->GetAsyncManager()->AddAsyncCompletionHandle(pipe);
    }
    else
    {
        ctx->SetIsConnectionFromPool(TRUE);
    }

    ctx->SetPipe(pipe);	

    ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(),
        L"iisnode created named pipe connection to the node.exe process", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

    CProtocolBridge::SendHttpRequestHeaders(ctx);

    return;

Error:

    if (INVALID_HANDLE_VALUE == pipe) 
    {
        if (retry >= CModuleConfiguration::GetMaxNamedPipeConnectionRetry(ctx->GetHttpContext()))
        {
            if (hr == ERROR_PIPE_BUSY)
            {
                ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(),
                    L"iisnode was unable to establish named pipe connection to the node.exe process because the named pipe server is too busy", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
                CProtocolBridge::SendEmptyResponse(ctx, 503, CNodeConstants::IISNODE_ERROR_PIPE_CONNECTION_TOO_BUSY, _T("Service Unavailable"), hr);
            }
            else
            {
                ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(),
                    L"iisnode was unable to establish named pipe connection to the node.exe process", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
                CProtocolBridge::SendEmptyResponse( ctx, 
                                                    500, 
                                                    CNodeConstants::IISNODE_ERROR_PIPE_CONNECTION, 
                                                    _T("Internal Server Error"), 
                                                    hr );
            }
        }
        else if (ctx->GetNodeProcess()->HasProcessExited()) 
        {
            // the process has exited, likely due to initialization error
            // stop trying to establish the named pipe connection to minimize the failure latency

            ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(),
                L"iisnode was unable to establish named pipe connection to the node.exe process before the process terminated", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
            CProtocolBridge::SendEmptyResponse( ctx, 
                                                500, 
                                                CNodeConstants::IISNODE_ERROR_PIPE_CONNECTION_BEFORE_PROCESS_TERMINATED, 
                                                _T("Internal Server Error"), 
                                                hr );
        }
        else 
        {
            ctx->SetConnectionRetryCount(retry + 1);
            ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(),
                L"iisnode scheduled a retry of a named pipe connection to the node.exe process ", WINEVENT_LEVEL_INFO, ctx->GetActivityId());
            CProtocolBridge::PostponeProcessing(ctx, CModuleConfiguration::GetNamedPipeConnectionRetryDelay(ctx->GetHttpContext()));
        }
    }
    else
    {
        CloseHandle(pipe);
        pipe = INVALID_HANDLE_VALUE;
        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(),
            L"iisnode was unable to configure the named pipe connection to the node.exe process", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
        CProtocolBridge::SendEmptyResponse( ctx, 
                                            500, 
                                            CNodeConstants::IISNODE_ERROR_CONFIGURE_PIPE_CONNECTION, 
                                            _T("Internal Server Error"), 
                                            hr );
    }

    return;
}

void CProtocolBridge::SendHttpRequestHeaders(CNodeHttpStoredContext* context)
{
    HRESULT hr;
    DWORD length;
    IHttpRequest *request;
    PCSTR pszConnectionHeader = NULL;

    // set the start time of the request

    GetSystemTimeAsFileTime(context->GetStartTime());

    // capture ETW provider since after a successful call to WriteFile the context may be asynchronously deleted

    CNodeEventProvider* etw = context->GetNodeApplication()->GetApplicationManager()->GetEventProvider();
    GUID activityId;
    memcpy(&activityId, context->GetActivityId(), sizeof GUID);

    // request the named pipe to be kept alive by the server after the response is sent
    // to enable named pipe connection pooling

    request = context->GetHttpContext()->GetRequest();

    pszConnectionHeader = request->GetHeader(HttpHeaderConnection);
    if( pszConnectionHeader == NULL || 
        (pszConnectionHeader != NULL && stricmp(pszConnectionHeader, "upgrade") != 0))
    {
        CheckError(request->SetHeader(HttpHeaderConnection, "keep-alive", 10, TRUE));
    }

    // Expect: 100-continue has been processed by IIS - do not propagate it up to node.js since node will
    // attempt to process it again

    USHORT expectLength;
    PCSTR expect = request->GetHeader(HttpHeaderExpect, &expectLength);
    if (NULL != expect && 0 == strnicmp(expect, "100-continue", expectLength))
    {
        CheckError(request->DeleteHeader(HttpHeaderExpect));
    }

    // determine if the request body had been chunked; IIS decodes chunked encoding, so it
    // must be re-applied when sending the request entity body

    USHORT encodingLength;
    PCSTR encoding = request->GetHeader(HttpHeaderTransferEncoding, &encodingLength);
    if (NULL != encoding && 0 == strnicmp(encoding, "chunked;", encodingLength > 8 ? 8 : encodingLength))
    {
        context->SetIsChunked(TRUE);
        context->SetIsLastChunk(FALSE);
    }

    // serialize and send request headers

    CheckError(CHttpProtocol::SerializeRequestHeaders(context, context->GetBufferRef(), context->GetBufferSizeRef(), &length));

    context->SetNextProcessor(CProtocolBridge::SendHttpRequestHeadersCompleted);
    
    if (WriteFile(context->GetPipe(), context->GetBuffer(), length, NULL, context->InitializeOverlapped()))
    {
        // completed synchronously

        etw->Log(context->GetHttpContext(), L"iisnode initiated sending http request headers to the node.exe process and completed synchronously", 
            WINEVENT_LEVEL_VERBOSE, 
            &activityId);

        // despite IO completion ports are used, asynchronous callback will not be invoked because in 
        // CProtocolBridge:CreateNamedPipeConnection the SetFileCompletionNotificationModes function was called
        // - see http://msdn.microsoft.com/en-us/library/windows/desktop/aa365683(v=vs.85).aspx
        // and http://msdn.microsoft.com/en-us/library/windows/desktop/aa365538(v=vs.85).aspx

        CProtocolBridge::SendHttpRequestHeadersCompleted(S_OK, 0, context->GetOverlapped());
    }
    else 
    {
        hr = GetLastError();
        if (ERROR_IO_PENDING == hr)
        {
        }
        else 
        {
            // error

            if (context->GetIsConnectionFromPool())
            {
                // communication over a connection from the connection pool failed
                // try to create a brand new connection instead

                context->SetConnectionRetryCount(1);
                CProtocolBridge::CreateNamedPipeConnection(S_OK, 0, context->GetOverlapped());
            }
            else
            {
                etw->Log(context->GetHttpContext(), L"iisnode failed to initiate sending http request headers to the node.exe process", 
                    WINEVENT_LEVEL_ERROR, 
                    &activityId);

                CProtocolBridge::SendEmptyResponse( context, 
                                                    500, 
                                                    CNodeConstants::IISNODE_ERROR_FAILED_INIT_SEND_HTTP_HEADERS, 
                                                    _T("Internal Server Error"), 
                                                    hr );
            }
        }
    }

    return;

Error:

    etw->Log(context->GetHttpContext(), L"iisnode failed to serialize http request headers", 
        WINEVENT_LEVEL_ERROR, 
        &activityId);

    CProtocolBridge::SendEmptyResponse( context, 
                                        500, 
                                        CNodeConstants::IISNODE_ERROR_FAILED_SERIALIZE_HTTP_HEADERS, 
                                        _T("Internal Server Error"), 
                                        hr );

    return;
}

void WINAPI CProtocolBridge::SendHttpRequestHeadersCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
    HRESULT hr;
    CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

    CheckError(error);
    ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
        L"iisnode finished sending http request headers to the node.exe process", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
    CProtocolBridge::ReadRequestBody(ctx);

    return;
Error:

    if (ctx->GetIsConnectionFromPool())
    {
        // communication over a connection from the connection pool failed
        // try to create a brand new connection instead

        ctx->SetConnectionRetryCount(1);
        CProtocolBridge::CreateNamedPipeConnection(S_OK, 0, ctx->GetOverlapped());
    }
    else
    {
        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
            L"iisnode failed to send http request headers to the node.exe process", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
        CProtocolBridge::SendEmptyResponse( ctx, 
                                            500, 
                                            CNodeConstants::IISNODE_ERROR_FAILED_SEND_HTTP_HEADERS, 
                                            _T("Internal Server Error"), 
                                            hr );
    }

    return;
}

void CProtocolBridge::ReadRequestBody(CNodeHttpStoredContext* context)
{
    HRESULT hr;	
    DWORD bytesReceived = 0;
    BOOL completionPending = FALSE;
    BOOL continueSynchronouslyNow = TRUE;

    if (0 < context->GetHttpContext()->GetRequest()->GetRemainingEntityBytes() || context->GetIsUpgrade())
    {
        context->SetNextProcessor(CProtocolBridge::ReadRequestBodyCompleted);
        
        if (context->GetIsChunked())
        {
            CheckError(context->GetHttpContext()->GetRequest()->ReadEntityBody(context->GetChunkBuffer(), context->GetChunkBufferSize(), TRUE, &bytesReceived, &completionPending));
        }
        else
        {
            CheckError(context->GetHttpContext()->GetRequest()->ReadEntityBody(context->GetBuffer(), context->GetBufferSize(), TRUE, &bytesReceived, &completionPending));
        }

        if (!completionPending)
        {
            context->SetContinueSynchronously(TRUE);
            continueSynchronouslyNow = FALSE;
            context->SetBytesCompleted(bytesReceived);
        }
    }

    if (!completionPending)
    {
        context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(context->GetHttpContext(),
            L"iisnode initiated reading http request body chunk and completed synchronously", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());

        context->SetBytesCompleted(bytesReceived);
        if (continueSynchronouslyNow)
        {
            CProtocolBridge::ReadRequestBodyCompleted(S_OK, 0, context->GetOverlapped());
        }
    }
    else
    {
        //context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(context->GetHttpContext(),
        //    L"iisnode initiated reading http request body chunk and will complete asynchronously", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());
    }

    return;
Error:

    if (HRESULT_FROM_WIN32(ERROR_HANDLE_EOF) == hr)
    {
        context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(context->GetHttpContext(),
            L"iisnode detected the end of the http request body", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());

        if (context->GetIsUpgrade())
        {
            CProtocolBridge::FinalizeUpgradeResponse(context, S_OK);
        }
        else if (context->GetIsChunked() && !context->GetIsLastChunk())
        {
            // send the terminating zero-length chunk

            CProtocolBridge::ReadRequestBodyCompleted(S_OK, 0, context->GetOverlapped());
        }
        else
        {
            CProtocolBridge::StartReadResponse(context);
        }
    }
    else
    {
        context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(context->GetHttpContext(),
            L"iisnode failed reading http request body", WINEVENT_LEVEL_ERROR, context->GetActivityId());

        if (context->GetIsUpgrade()) 
        {
            CProtocolBridge::FinalizeUpgradeResponse(context, HRESULT_FROM_WIN32(hr));
        }
        else
        {
            CProtocolBridge::SendEmptyResponse( context, 
                                                500, 
                                                CNodeConstants::IISNODE_ERROR_FAILED_READ_REQ_BODY, 
                                                _T("Internal Server Error"), 
                                                HRESULT_FROM_WIN32(hr) );
        }
    }

    return;
}

void WINAPI CProtocolBridge::ReadRequestBodyCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{	
    CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

    if (S_OK == error && bytesTransfered > 0)
    {
        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
            L"iisnode read a chunk of http request body", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
        CProtocolBridge::SendRequestBody(ctx, bytesTransfered);
    }
    else if (ERROR_HANDLE_EOF == error || 0 == bytesTransfered)
    {	
        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
            L"iisnode detected the end of the http request body", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

        if (ctx->GetIsUpgrade()) 
        {
            CProtocolBridge::FinalizeUpgradeResponse(ctx, S_OK);
        }
        else if (ctx->GetIsChunked() && !ctx->GetIsLastChunk()) 
        {
            // send the zero-length last chunk to indicate the end of a chunked entity body

            CProtocolBridge::SendRequestBody(ctx, 0);
        }
        else
        {
            CProtocolBridge::StartReadResponse(ctx);
        }
    }
    else 
    {
        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
            L"iisnode failed reading http request body", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());

        if (ctx->GetIsUpgrade())
        {
            CProtocolBridge::FinalizeUpgradeResponse(ctx, error);
        }
        else
        {
            CProtocolBridge::SendEmptyResponse( ctx, 
                                                500, 
                                                CNodeConstants::IISNODE_ERROR_FAILED_READ_REQ_BODY_COMPLETED, 
                                                _T("Internal Server Error"), 
                                                error );
        }
    }
}

void CProtocolBridge::SendRequestBody(CNodeHttpStoredContext* context, DWORD chunkLength)
{
    // capture ETW provider since after a successful call to WriteFile the context may be asynchronously deleted

    CNodeEventProvider* etw = context->GetNodeApplication()->GetApplicationManager()->GetEventProvider();
    GUID activityId;
    memcpy(&activityId, context->GetActivityId(), sizeof GUID);

    DWORD length;
    char* buffer;

    if (context->GetIsChunked())
    {
        // IIS decodes chunked transfer encoding of request entity body. Chunked encoding must be 
        // re-applied here around the request body data IIS provided in the buffer before it is sent to node.exe.
        // This is done by calculating and pre-pending the chunk header to the data in the buffer 
        // and appending a chunk terminating CRLF to the data in the buffer. 

        // http://www.w3.org/Protocols/rfc2616/rfc2616-sec3.html#sec3.6

        // Generate the chunk header (from last byte to first)

        buffer = (char*)context->GetChunkBuffer(); // first byte of entity body chunk data
        *(--buffer) = 0x0A; // LF
        *(--buffer) = 0x0D; // CR

        if (0 == chunkLength) 
        {
            // this is the end of the request entity body - generate last, zero-length chunk
            *(--buffer) = '0';
            context->SetIsLastChunk(TRUE);
        }
        else 
        {
            length = chunkLength;
            while (length > 0)
            {
                DWORD digit = length % 16;
                *(--buffer) = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
                length >>= 4;
            }
        }

        // Append CRLF to the entity body chunk

        char* end = (char*)context->GetChunkBuffer() + chunkLength; // first byte after the chunk data
        *end = 0x0D; // CR
        *(++end) = 0x0A; // LF

        // Calculate total length of the chunk including framing

        length = end - buffer + 1;
    }
    else
    {
        length = chunkLength;
        buffer = (char*)context->GetBuffer();
    }

    // send the entity body data to the node.exe process

    context->SetNextProcessor(CProtocolBridge::SendRequestBodyCompleted);

    if (WriteFile(context->GetPipe(), (void*)buffer, length, NULL, context->InitializeOverlapped()))
    {
        // completed synchronously

        etw->Log(context->GetHttpContext(), L"iisnode initiated sending http request body chunk to the node.exe process and completed synchronously", 
            WINEVENT_LEVEL_VERBOSE, 
            &activityId);

        // despite IO completion ports are used, asynchronous callback will not be invoked because in 
        // CProtocolBridge:CreateNamedPipeConnection the SetFileCompletionNotificationModes function was called
        // - see http://msdn.microsoft.com/en-us/library/windows/desktop/aa365683(v=vs.85).aspx
        // and http://msdn.microsoft.com/en-us/library/windows/desktop/aa365538(v=vs.85).aspx

        CProtocolBridge::SendRequestBodyCompleted(S_OK, 0, context->GetOverlapped());
    }
    else 
    {
        HRESULT hr = GetLastError();

        if (ERROR_IO_PENDING == hr)
        {
            // will complete asynchronously

            //etw->Log(context->GetHttpContext(), L"iisnode initiated sending http request body chunk to the node.exe process and will complete asynchronously", 
            //    WINEVENT_LEVEL_VERBOSE, 
            //    &activityId);
        }
        else if (ERROR_NO_DATA == hr)
        {
            // Node.exe has closed the named pipe. This means it does not expect any more request data, but it does not mean there is no response.
            // This may happen even for POST requests if the node.js application does not register event handlers for the 'data' or 'end' request events.
            // Ignore the write error and attempt to read the response instead (which might have been written by node.exe before the named pipe connection 
            // was closed). This may also happen for WebSocket traffic. 

            etw->Log(context->GetHttpContext(), L"iisnode detected the node.exe process closed the named pipe connection", 
                WINEVENT_LEVEL_VERBOSE, 
                &activityId);

            if (context->GetIsUpgrade()) 
            {
                CProtocolBridge::FinalizeUpgradeResponse(context, S_OK);
            }
            else
            {
                CProtocolBridge::StartReadResponse(context);
            }
        }
        else
        {
            // error

            etw->Log(context->GetHttpContext(), L"iisnode failed to initiate sending http request body chunk to the node.exe process", 
                WINEVENT_LEVEL_ERROR, 
                &activityId);

            if (context->GetIsUpgrade())
            {
                CProtocolBridge::FinalizeUpgradeResponse(context, hr);
            }
            else
            {
                CProtocolBridge::SendEmptyResponse( context, 
                                                    500, 
                                                    CNodeConstants::IISNODE_ERROR_FAILED_INIT_SEND_REQ_BODY, 
                                                    _T("Internal Server Error"), 
                                                    hr );
            }
        }
    }

    return;
}

void WINAPI CProtocolBridge::SendRequestBodyCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
    CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

    if (S_OK == error)
    {
        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(),
            L"iisnode finished sending http request body chunk to the node.exe process", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
        CProtocolBridge::ReadRequestBody(ctx);
    }
    else 
    {
        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(),
            L"iisnode failed to send http request body chunk to the node.exe process", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());

        if (ctx->GetIsUpgrade())
        {
            CProtocolBridge::FinalizeUpgradeResponse(ctx, error);
        }
        else
        {
            CProtocolBridge::SendEmptyResponse( ctx, 
                                                500, 
                                                CNodeConstants::IISNODE_ERROR_FAILED_SEND_REQ_BODY, 
                                                _T("Internal Server Error"), 
                                                error );
        }
    }
}

void CProtocolBridge::StartReadResponse(CNodeHttpStoredContext* context)
{
    context->SetDataSize(0);
    context->SetParsingOffset(0);
    context->SetNextProcessor(CProtocolBridge::ProcessResponseStatusLine);
    context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(context->GetHttpContext(),
        L"iisnode starting to read http response", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());
    CProtocolBridge::ContinueReadResponse(context);
}

HRESULT CProtocolBridge::EnsureBuffer(CNodeHttpStoredContext* context)
{
    HRESULT hr;

    if (context->GetBufferSize() == context->GetDataSize())
    {
        // buffer is full

        if (context->GetParsingOffset() > 0)
        {
            // remove already parsed data from buffer by moving unprocessed data to the front; this will free up space at the end

            char* b = (char*)context->GetBuffer();
            memcpy(b, b + context->GetParsingOffset(), context->GetDataSize() - context->GetParsingOffset());
        }
        else 
        {
            // allocate more buffer memory

            context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(context->GetHttpContext(),
                L"iisnode allocating more buffer memory to handle http response", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());

            DWORD* bufferLength = context->GetBufferSizeRef();
            void** buffer = context->GetBufferRef();

            DWORD quota = CModuleConfiguration::GetMaxRequestBufferSize(context->GetHttpContext());
            ErrorIf(*bufferLength >= quota, ERROR_NOT_ENOUGH_QUOTA);

            void* newBuffer;
            DWORD newBufferLength = *bufferLength * 2;
            if (newBufferLength > quota)
            {
                newBufferLength = quota;
            }

            ErrorIf(NULL == (newBuffer = context->GetHttpContext()->AllocateRequestMemory(newBufferLength)), ERROR_NOT_ENOUGH_MEMORY);
            memcpy(newBuffer, (char*)(*buffer) + context->GetParsingOffset(), context->GetDataSize() - context->GetParsingOffset());
            *buffer = newBuffer;
            *bufferLength = newBufferLength;
        }

        context->SetDataSize(context->GetDataSize() - context->GetParsingOffset());
        context->SetParsingOffset(0);
    }

    return S_OK;
Error:
    return hr;

}

void CProtocolBridge::ContinueReadResponse(CNodeHttpStoredContext* context)
{
    HRESULT hr;
    DWORD bytesRead = 0;

    // capture ETW provider since after a successful call to ReadFile the context may be asynchronously deleted

    CNodeEventProvider* etw = context->GetNodeApplication()->GetApplicationManager()->GetEventProvider();
    GUID activityId;
    memcpy(&activityId, context->GetActivityId(), sizeof GUID);

    CheckError(CProtocolBridge::EnsureBuffer(context));

    if (ReadFile(
            context->GetPipe(), 
            (char*)context->GetBuffer() + context->GetDataSize(), 
            context->GetBufferSize() - context->GetDataSize(),
            &bytesRead,
            context->InitializeOverlapped()))
    {
        // read completed synchronously 

        etw->Log(context->GetHttpContext(), L"iisnode initiated reading http response chunk and completed synchronously", 
            WINEVENT_LEVEL_VERBOSE, 
            &activityId);

        // despite IO completion ports are used, asynchronous callback will not be invoked because in 
        // CProtocolBridge:CreateNamedPipeConnection the SetFileCompletionNotificationModes function was called
        // - see http://msdn.microsoft.com/en-us/library/windows/desktop/aa365683(v=vs.85).aspx
        // and http://msdn.microsoft.com/en-us/library/windows/desktop/aa365538(v=vs.85).aspx

        context->GetAsyncContext()->completionProcessor(S_OK, bytesRead, context->GetOverlapped());
    }
    else if (ERROR_IO_PENDING == (hr = GetLastError()))
    {
        // read will complete asynchronously

        //etw->Log(context->GetHttpContext(), L"iisnode initiated reading http response chunk and will complete asynchronously", 
        //    WINEVENT_LEVEL_VERBOSE, 
        //    &activityId);
    }
    else if (ERROR_BROKEN_PIPE == hr && context->GetCloseConnection())
    {
        // Termination of a connection indicates the end of the response body if Connection: close response header was present

        CProtocolBridge::FinalizeResponse(context);
    }
    else
    {
        // error

        etw->Log(context->GetHttpContext(), L"iisnode failed to initialize reading of http response chunk", 
            WINEVENT_LEVEL_ERROR, 
            &activityId);

        CProtocolBridge::SendEmptyResponse( context, 
                                            500, 
                                            CNodeConstants::IISNODE_ERROR_FAILED_INIT_READ_RESPONSE, 
                                            _T("Internal Server Error"), 
                                            hr );
    }

    return;
Error:

    context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(context->GetHttpContext(), 
        L"iisnode failed to allocate memory buffer to read http response chunk", WINEVENT_LEVEL_ERROR, &activityId);

    CProtocolBridge::SendEmptyResponse( context, 
                                        500, 
                                        CNodeConstants::IISNODE_ERROR_FAILED_ALLOC_MEM_READ_RESPONSE, 
                                        _T("Internal Server Error"), 
                                        hr );

    return;
}

void WINAPI CProtocolBridge::ProcessResponseStatusLine(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
    HRESULT hr;
    CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

    ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
        L"iisnode starting to process http response status line", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

    CheckError(error);
    ctx->SetDataSize(ctx->GetDataSize() + bytesTransfered);
    CheckError(CHttpProtocol::ParseResponseStatusLine(ctx));

    ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
        L"iisnode finished processing http response status line", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

    ctx->SetNextProcessor(CProtocolBridge::ProcessResponseHeaders);
    CProtocolBridge::ProcessResponseHeaders(S_OK, 0, ctx->GetOverlapped());

    return;
Error:

    if (ERROR_MORE_DATA == hr)
    {
        CProtocolBridge::ContinueReadResponse(ctx);
    }
    else
    {
        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
            L"iisnode failed to process http response status line", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
        CProtocolBridge::SendEmptyResponse( ctx, 
                                            500, 
                                            CNodeConstants::IISNODE_ERROR_FAILED_PROCESS_HTTP_STATUS_LINE, 
                                            _T("Internal Server Error"), 
                                            hr );
    }

    return;
}

HRESULT CProtocolBridge::AddDebugHeader(CNodeHttpStoredContext* context)
{
    HRESULT hr;
    char* header;
    char buffer[256];

    ErrorIf(NULL == (header = (char*)context->GetHttpContext()->AllocateRequestMemory(512)), ERROR_NOT_ENOUGH_MEMORY);
    *header = 0;

    // iisnode version

    strcat(header, "http://bit.ly/NsU2nd#iisnode_ver=");
    strcat(header, IISNODE_VERSION);

    // node process command line

    WCHAR* npcl = CModuleConfiguration::GetNodeProcessCommandLine(context->GetHttpContext());	
    size_t npcl_length = wcslen(npcl) + 1;
    char* npcl_c;
    ErrorIf(NULL == (npcl_c = (char*)context->GetHttpContext()->AllocateRequestMemory(npcl_length)), ERROR_NOT_ENOUGH_MEMORY);
    size_t converted;
    wcstombs_s(&converted, 	npcl_c, npcl_length, npcl, _TRUNCATE);
    strcat(header, "&node=");
    strcat(header, npcl_c);

    // dns name

    DWORD length = sizeof buffer;
    if (GetComputerNameEx(ComputerNameDnsFullyQualified, buffer, &length))
    {
        strcat(header, "&dns=");
        strcat(header, buffer);
    }

    // worker PID

    sprintf(buffer, "&worker_pid=%d", GetCurrentProcessId());
    strcat(header, buffer);

    // node PID

    sprintf(buffer, "&node_pid=%d", context->GetNodeProcess()->GetPID());
    strcat(header, buffer);

    // worker working set and private bytes

    PROCESS_MEMORY_COUNTERS_EX memory;
    ErrorIf(!GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&memory, sizeof memory), GetLastError());
    sprintf(buffer, "&worker_mem_ws=%u&worker_mem_pagefile=%u", memory.WorkingSetSize / 1024, memory.PagefileUsage / 1024);
    strcat(header, buffer);

    // node working set and private bytes

    ErrorIf(!GetProcessMemoryInfo(context->GetNodeProcess()->GetProcess(), (PROCESS_MEMORY_COUNTERS*)&memory, sizeof memory), GetLastError());
    sprintf(buffer, "&node_mem_ws=%u&node_mem_pagefile=%u", memory.WorkingSetSize / 1024, memory.PagefileUsage / 1024);
    strcat(header, buffer);

    // number of processes

    sprintf(buffer, "&app_processes=%d", context->GetNodeApplication()->GetProcessCount());
    strcat(header, buffer);

    // process active requests

    sprintf(buffer, "&process_active_req=%d", context->GetNodeProcess()->GetActiveRequestCount());
    strcat(header, buffer);

    // application active requests

    sprintf(buffer, "&app_active_req=%d", context->GetNodeApplication()->GetActiveRequestCount());
    strcat(header, buffer);

    // worker total requests

    sprintf(buffer, "&worker_total_req=%d", context->GetNodeApplication()->GetApplicationManager()->GetTotalRequests());
    strcat(header, buffer);

    // named pipe reconnect attempts

    sprintf(buffer, "&np_retry=%d", context->GetConnectionRetryCount());
    strcat(header, buffer);

    // request time

    FILETIME endTime;
    GetSystemTimeAsFileTime(&endTime);
    ULARGE_INTEGER start, end;
    start.HighPart = context->GetStartTime()->dwHighDateTime;
    start.LowPart = context->GetStartTime()->dwLowDateTime;
    end.HighPart = endTime.dwHighDateTime;
    end.LowPart = endTime.dwLowDateTime;
    end.QuadPart -= start.QuadPart;
    end.QuadPart /= 10000; // convert to milliseconds
    sprintf(buffer, "&req_time=%u", end.LowPart);
    strcat(header, buffer);

    // hresult

    sprintf(buffer, "&hresult=%d", context->GetHresult());
    strcat(header, buffer);

    // add header

    CheckError(context->GetHttpContext()->GetResponse()->SetHeader("iisnode-debug", header, strlen(header), TRUE));

    return S_OK;

Error:

    return hr;
}

void WINAPI CProtocolBridge::ProcessResponseHeaders(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
    HRESULT hr;
    CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);
    PCSTR contentLength;
    USHORT contentLengthLength;

    ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
        L"iisnode starting to process http response headers", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

    CheckError(error);
    ctx->SetDataSize(ctx->GetDataSize() + bytesTransfered);
    CheckError(CHttpProtocol::ParseResponseHeaders(ctx));

    if (CModuleConfiguration::GetDebugHeaderEnabled(ctx->GetHttpContext()))
    {
        CheckError(CProtocolBridge::AddDebugHeader(ctx));
    }

    if (!ctx->GetExpectResponseBody())
    {
        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
            L"iisnode determined the HTTP response does not have entity body", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

        CProtocolBridge::FinalizeResponse(ctx);
    }
    else
    {
        if (ctx->GetIsUpgrade())
        {
            ctx->SetIsChunked(FALSE);
            ctx->SetChunkLength(MAXLONGLONG);
            ctx->SetIsLastChunk(TRUE);
            ctx->SetNextProcessor(CProtocolBridge::ProcessUpgradeResponse);
        }
        else if (ctx->GetCloseConnection())
        {
            ctx->SetIsChunked(FALSE);
            ctx->SetChunkLength(MAXLONGLONG);
            ctx->SetIsLastChunk(TRUE);
            ctx->SetNextProcessor(CProtocolBridge::ProcessResponseBody);
        }
        else 
        {
            contentLength = ctx->GetHttpContext()->GetResponse()->GetHeader(HttpHeaderContentLength, &contentLengthLength);
            if (0 == contentLengthLength)
            {
                ctx->SetIsChunked(TRUE);
                ctx->SetIsLastChunk(FALSE);
                ctx->SetNextProcessor(CProtocolBridge::ProcessChunkHeader);
            }
            else
            {
                LONGLONG length = 0;
                int i = 0;

                // skip leading white space
                while (i < contentLengthLength && (contentLength[i] < '0' || contentLength[i] > '9')) 
                    i++;

                while (i < contentLengthLength && contentLength[i] >= '0' && contentLength[i] <= '9') 
                    length = length * 10 + contentLength[i++] - '0';

                ctx->SetIsChunked(FALSE);
                ctx->SetIsLastChunk(TRUE);
                ctx->SetChunkLength(length);
                ctx->SetNextProcessor(CProtocolBridge::ProcessResponseBody);
            }
        }

        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
            L"iisnode finished processing http response headers", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

        ctx->GetAsyncContext()->completionProcessor(S_OK, 0, ctx->GetOverlapped());
    }

    return;
Error:

    if (ERROR_MORE_DATA == hr)
    {
        CProtocolBridge::ContinueReadResponse(ctx);
    }
    else
    {
        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
            L"iisnode failed to process http response headers", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
        CProtocolBridge::SendEmptyResponse( ctx, 
                                            500, 
                                            CNodeConstants::IISNODE_ERROR_FAILED_PROCESS_HTTP_HEADERS, 
                                            _T("Internal Server Error"), 
                                            hr );
    }

    return;
}

void WINAPI CProtocolBridge::ProcessChunkHeader(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
    HRESULT hr;
    CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

    ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
        L"iisnode starting to process http response body chunk header", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

    CheckError(error);

    ctx->SetDataSize(ctx->GetDataSize() + bytesTransfered);
    CheckError(CHttpProtocol::ParseChunkHeader(ctx));

    ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
        L"iisnode finished processing http response body chunk header", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

    ctx->SetNextProcessor(CProtocolBridge::ProcessResponseBody);
    CProtocolBridge::ProcessResponseBody(S_OK, 0, ctx->GetOverlapped());

    return;

Error:

    if (ERROR_MORE_DATA == hr)
    {
        CProtocolBridge::ContinueReadResponse(ctx);
    }
    else
    {
        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
            L"iisnode failed to process response body chunk header", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
        CProtocolBridge::SendEmptyResponse( ctx, 
                                            500, 
                                            CNodeConstants::IISNODE_ERROR_FAILED_PROCESS_RESPONSE_BODY_CHUNK_HEADER, 
                                            _T("Internal Server Error"), 
                                            hr );
    }

    return;
}

void CProtocolBridge::EnsureRequestPumpStarted(CNodeHttpStoredContext* context) 
{
    if (context->GetOpaqueFlagSet() && !context->GetRequestPumpStarted())
    {
        // Ensure that we start reading the request of an HTTP Upgraded request 
        // only after the 101 Switching Protocols response had been sent. 

        context->SetRequestPumpStarted();
        CProtocolBridge::ReadRequestBody(context->GetUpgradeContext());
        ASYNC_CONTEXT* async = context->GetUpgradeContext()->GetAsyncContext();
        async->RunSynchronousContinuations();
    }
}

void WINAPI CProtocolBridge::ProcessResponseBody(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
    HRESULT hr;
    CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);
    HTTP_DATA_CHUNK* chunk;
    DWORD bytesSent;
    BOOL completionExpected;

    ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
        L"iisnode starting to process http response body", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());

    CheckError(error);

    ctx->SetDataSize(ctx->GetDataSize() + bytesTransfered);

    if (ctx->GetDataSize() > ctx->GetParsingOffset())
    {
        // there is response body data in the buffer

        if (ctx->GetChunkLength() > ctx->GetChunkTransmitted())
        {
            // send the smaller of the rest of the current chunk or the data available in the buffer to the client

            DWORD dataInBuffer = ctx->GetDataSize() - ctx->GetParsingOffset();
            DWORD remainingChunkSize = ctx->GetChunkLength() - ctx->GetChunkTransmitted();
            DWORD bytesToSend = dataInBuffer < remainingChunkSize ? dataInBuffer : remainingChunkSize;

            CheckError(ctx->EnsureResponseChunk(bytesToSend, &chunk));
            chunk->FromMemory.BufferLength = bytesToSend;
            memcpy(chunk->FromMemory.pBuffer, (char*)ctx->GetBuffer() + ctx->GetParsingOffset(), chunk->FromMemory.BufferLength);

            if (bytesToSend == dataInBuffer)
            {
                ctx->SetDataSize(0);
                ctx->SetParsingOffset(0);
            }
            else
            {
                ctx->SetParsingOffset(ctx->GetParsingOffset() + bytesToSend);
            }

            ctx->SetNextProcessor(CProtocolBridge::SendResponseBodyCompleted);
            ctx->SetBytesCompleted(bytesToSend);

            CheckError(ctx->GetHttpContext()->GetResponse()->WriteEntityChunks(
                chunk,
                1,
                TRUE,
                !ctx->GetIsLastChunk() || remainingChunkSize > bytesToSend,
                &bytesSent,
                &completionExpected));

            ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
                L"iisnode started sending http response body chunk", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
        
            if (!completionExpected)
            {
                ctx->SetContinueSynchronously(TRUE);
            }
        }
        else if (ctx->GetIsChunked())
        {
            // process next chunk of the chunked encoding

            ctx->SetNextProcessor(CProtocolBridge::ProcessChunkHeader);
            CProtocolBridge::ProcessChunkHeader(S_OK, 0, ctx->GetOverlapped());
        }
        else
        {
            // response data detected beyond the body length declared with Content-Length

            CheckError(ERROR_BAD_FORMAT);
        }
    }
    else if (ctx->GetIsChunked() || ctx->GetChunkLength() > ctx->GetChunkTransmitted())
    {
        // read more body data

        CProtocolBridge::ContinueReadResponse(ctx);
    }
    else
    {
        // finish request

        if (ctx->GetIsUpgrade())
        {
            CProtocolBridge::FinalizeUpgradeResponse(ctx, S_OK);
        }
        else
        {
            CProtocolBridge::FinalizeResponse(ctx);
        }
    }

    return;
Error:

    if (ERROR_BROKEN_PIPE == hr && ctx->GetIsUpgrade())
    {
        // Termination of a connection indicates the end of the upgraded request 

        CProtocolBridge::FinalizeUpgradeResponse(ctx, S_OK);
    }
    else if (ERROR_BROKEN_PIPE == hr && ctx->GetCloseConnection())
    {
        // Termination of a connection indicates the end of the response body if Connection: close response header was present

        CProtocolBridge::FinalizeResponse(ctx);
    }
    else
    {
        ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
            L"iisnode failed to send http response body chunk", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());

        if (ctx->GetIsUpgrade())
        {
            CProtocolBridge::FinalizeUpgradeResponse(ctx, hr);
        }
        else
        {
            CProtocolBridge::SendEmptyResponse( ctx, 
                                                500, 
                                                CNodeConstants::IISNODE_ERROR_FAILED_SEND_RESPONSE_BODY_CHUNK, 
                                                _T("Internal Server Error"), 
                                                hr );
        }
    }

    return;
}

void WINAPI CProtocolBridge::SendResponseBodyCompleted(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
    HRESULT hr;
    CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);
    DWORD bytesSent;
    BOOL completionExpected = FALSE;

    CheckError(error);

    if (!ctx->GetIsUpgrade())
    {		
        ctx->SetChunkTransmitted(ctx->GetChunkTransmitted() + bytesTransfered);
        if (ctx->GetChunkLength() == ctx->GetChunkTransmitted())
        {
            ctx->SetChunkTransmitted(0);
            ctx->SetChunkLength(0);
        }
    }

    if (ctx->GetIsLastChunk() && ctx->GetChunkLength() == ctx->GetChunkTransmitted())
    {
        CProtocolBridge::FinalizeResponse(ctx);
    }
    else
    {
        if (ctx->GetIsChunked() && CModuleConfiguration::GetFlushResponse(ctx->GetHttpContext()))
        {
            // Flushing of chunked responses is enabled

            ctx->SetNextProcessor(CProtocolBridge::ContinueProcessResponseBodyAfterPartialFlush);
            ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
                L"iisnode initiated flushing http response body chunk", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
            ctx->GetHttpContext()->GetResponse()->Flush(TRUE, TRUE, &bytesSent, &completionExpected);
        }

        if (!completionExpected)
        {
            CProtocolBridge::ContinueProcessResponseBodyAfterPartialFlush(S_OK, 0, ctx->GetOverlapped());
        }
    }

    return;
Error:

    ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
        L"iisnode failed to flush http response body chunk", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());

    if (ctx->GetIsUpgrade())
    {
        CProtocolBridge::FinalizeUpgradeResponse(ctx, hr);
    }
    else 
    {
        CProtocolBridge::SendEmptyResponse( ctx, 
                                            500, 
                                            CNodeConstants::IISNODE_ERROR_FAILED_FLUSH_RESPONSE_BODY, 
                                            _T("Internal Server Error"), 
                                            hr );
    }

    return;
}

void WINAPI CProtocolBridge::ProcessUpgradeResponse(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
    BOOL completionExpected;
    DWORD bytesSent;
    CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

    ctx->SetNextProcessor(CProtocolBridge::ContinueProcessResponseBodyAfterPartialFlush);
    ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
        L"iisnode initiated flushing http upgrade response headers", WINEVENT_LEVEL_VERBOSE, ctx->GetActivityId());
    ctx->GetHttpContext()->GetResponse()->Flush(TRUE, TRUE, &bytesSent, &completionExpected);

    if (!completionExpected)
    {
        CProtocolBridge::ContinueProcessResponseBodyAfterPartialFlush(S_OK, 0, ctx->GetOverlapped());
    }
}


void WINAPI CProtocolBridge::ContinueProcessResponseBodyAfterPartialFlush(DWORD error, DWORD bytesTransfered, LPOVERLAPPED overlapped)
{
    HRESULT hr;
    CNodeHttpStoredContext* ctx = CNodeHttpStoredContext::Get(overlapped);

    CheckError(error);	

    // Start reading the request bytes if the request was an accepted HTTP Upgrade
    CProtocolBridge::EnsureRequestPumpStarted(ctx);

    // Continue on to reading the response body
    ctx->SetNextProcessor(CProtocolBridge::ProcessResponseBody);
    CProtocolBridge::ProcessResponseBody(S_OK, 0, ctx->GetOverlapped());

    return;
Error:

    ctx->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(ctx->GetHttpContext(), 
        L"iisnode failed to flush http response body chunk", WINEVENT_LEVEL_ERROR, ctx->GetActivityId());
    CProtocolBridge::SendEmptyResponse( ctx, 
                                        500, 
                                        CNodeConstants::IISNODE_ERROR_FAILED_FLUSH_RESPONSE_BODY_PARTIAL_FLUSH, 
                                        _T("Internal Server Error"), 
                                        hr );

    return;
}

void CProtocolBridge::FinalizeUpgradeResponse(CNodeHttpStoredContext* context, HRESULT hresult)
{
    context->SetNextProcessor(NULL);
    context->SetHresult(hresult);	

    if (0 == context->DecreasePendingAsyncOperationCount())
    {
        context->GetNodeProcess()->OnRequestCompleted(context);

        context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(context->GetHttpContext(), 
            L"iisnode finished processing both directions of upgraded http request/response", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());

        context->SetRequestNotificationStatus(RQ_NOTIFICATION_CONTINUE);
        CloseHandle(context->GetPipe());
        context->SetPipe(INVALID_HANDLE_VALUE);
        context->GetHttpContext()->GetResponse()->SetNeedDisconnect();
        context->GetHttpContext()->PostCompletion(0);
    }
    else
    {
        context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(context->GetHttpContext(), 
            L"iisnode finished processing one direction of upgraded http request/response", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());

        context->SetRequestNotificationStatus(RQ_NOTIFICATION_PENDING);
    }
}

void CProtocolBridge::FinalizeResponse(CNodeHttpStoredContext* context)
{
    context->GetNodeApplication()->GetApplicationManager()->GetEventProvider()->Log(context->GetHttpContext(),

        L"iisnode finished processing http request/response", WINEVENT_LEVEL_VERBOSE, context->GetActivityId());

    if (context->GetCloseConnection())
    {
        CloseHandle(context->GetPipe());
    }
    else
    {
        context->GetNodeProcess()->GetConnectionPool()->Return(context->GetPipe());
    }

    context->SetPipe(INVALID_HANDLE_VALUE);
    CProtocolBridge::FinalizeResponseCore(
        context, 
        RQ_NOTIFICATION_CONTINUE, 
        S_OK, 
        context->GetNodeApplication()->GetApplicationManager()->GetEventProvider(),
        L"iisnode posts completion from FinalizeResponse", 
        WINEVENT_LEVEL_VERBOSE);
}

HRESULT CProtocolBridge::FinalizeResponseCore(CNodeHttpStoredContext* context, REQUEST_NOTIFICATION_STATUS status, HRESULT error, CNodeEventProvider* log, PCWSTR etw, UCHAR level)
{
    context->SetRequestNotificationStatus(status);
    context->SetNextProcessor(NULL);
    context->SetHresult(error);

    if (NULL != context->GetNodeProcess())
    {
        // there is no CNodeProcess assigned to the request yet - something failed before it was moved from the pending queue
        // to an active request queue of a specific process

        context->GetNodeProcess()->OnRequestCompleted(context);
    }

    if (0 == context->DecreasePendingAsyncOperationCount()) // decreases ref count increased in the ctor of CNodeApplication::Dispatch
    {
        log->Log(etw, level, context->GetActivityId());	
        context->GetHttpContext()->PostCompletion(0);
    }

    return S_OK;
}

HRESULT CProtocolBridge::SendDebugRedirect(CNodeHttpStoredContext* context, CNodeEventProvider* log)
{
    HRESULT hr;

    IHttpContext* ctx = context->GetHttpContext();
    IHttpResponse* response = ctx->GetResponse();
    HTTP_REQUEST* raw = ctx->GetRequest()->GetRawHttpRequest();

    // redirect app.js/debug to app.js/debug/ by appending '/' at the end of the path

    PSTR path;
    int pathSizeA;
    ErrorIf(0 == (pathSizeA = WideCharToMultiByte(CP_ACP, 0, raw->CookedUrl.pAbsPath, raw->CookedUrl.AbsPathLength >> 1, NULL, 0, NULL, NULL)), E_FAIL);
    ErrorIf(NULL == (path = (TCHAR*)ctx->AllocateRequestMemory(pathSizeA + 2)), ERROR_NOT_ENOUGH_MEMORY);
    ErrorIf(pathSizeA != WideCharToMultiByte(CP_ACP, 0, raw->CookedUrl.pAbsPath, raw->CookedUrl.AbsPathLength >> 1, path, pathSizeA, NULL, NULL), E_FAIL);
    path[pathSizeA] = '/';
    path[pathSizeA + 1] = 0;

    response->SetStatus(301, "Moved Permanently"); 
    CheckError(context->GetHttpContext()->GetResponse()->Redirect(path, FALSE, TRUE));
    CProtocolBridge::FinalizeResponseCore(
        context, 
        RQ_NOTIFICATION_FINISH_REQUEST, 
        S_OK, 
        log,
        L"iisnode redirected debugging request", 
        WINEVENT_LEVEL_VERBOSE);

    return S_OK;
Error:
    return hr;
}