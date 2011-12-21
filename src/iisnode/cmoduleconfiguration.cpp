#include "precomp.h"

IHttpServer* CModuleConfiguration::server = NULL;

HTTP_MODULE_ID CModuleConfiguration::moduleId = NULL;

CModuleConfiguration::CModuleConfiguration()
	: nodeProcessCommandLine(NULL), logDirectoryNameSuffix(NULL), debuggerPathSegment(NULL), 
	debugPortRange(NULL), debugPortStart(0), debugPortEnd(0), node_env(NULL)
{
}

CModuleConfiguration::~CModuleConfiguration()
{	
	if (NULL != this->nodeProcessCommandLine)
	{
		delete [] this->nodeProcessCommandLine;
		this->nodeProcessCommandLine = NULL;
	}

	if (NULL != this->logDirectoryNameSuffix)
	{
		delete [] this->logDirectoryNameSuffix;
		this->logDirectoryNameSuffix = NULL;
	}

	if (NULL != this->debuggerPathSegment)
	{
		delete [] this->debuggerPathSegment;
		this->debuggerPathSegment = NULL;
	}

	if (NULL != this->node_env)
	{
		delete this->node_env;
		this->node_env = NULL;
	}
}

HRESULT CModuleConfiguration::Initialize(IHttpServer* server, HTTP_MODULE_ID moduleId)	
{
	HRESULT hr;

	CheckNull(server);
	CheckNull(moduleId);

	CModuleConfiguration::server = server;
	CModuleConfiguration::moduleId = moduleId;

	return S_OK;
Error:
	return hr;
}

HRESULT CModuleConfiguration::CreateNodeEnvironment(IHttpContext* ctx, DWORD debugPort, PCH namedPipe, PCH* env)
{
	HRESULT hr;
	LPCH currentEnvironment = NULL;
	LPCH tmpStart, tmpIndex = NULL;
	DWORD tmpSize;
	DWORD environmentSize;
	IAppHostElement* section = NULL;
	IAppHostElementCollection* appSettings = NULL;
	IAppHostElement* entry = NULL;
	IAppHostPropertyCollection* properties = NULL;
	IAppHostProperty* prop = NULL;
	BSTR keyPropertyName = NULL;
	BSTR valuePropertyName = NULL;
	VARIANT vKeyPropertyName;
	VARIANT vValuePropertyName;
	DWORD count;
	BSTR propertyValue = NULL;
	int propertySize;

	CheckNull(env);
	*env = NULL;

	// this is a zero terminated list of zero terminated strings of the form <var>=<value>

	// calculate size of current environment

	ErrorIf(NULL == (currentEnvironment = GetEnvironmentStrings()), GetLastError());
	environmentSize = 0;
	do {
		while (*(currentEnvironment + environmentSize++) != 0);
	} while (*(currentEnvironment + environmentSize++) != 0);

	// allocate memory for new environment variables

	tmpSize = 32767 - environmentSize;
	ErrorIf(NULL == (tmpIndex = tmpStart = new char[tmpSize]), ERROR_NOT_ENOUGH_MEMORY);
	RtlZeroMemory(tmpIndex, tmpSize);

	// set PORT and IISNODE_VERSION variables

	ErrorIf(tmpSize < (strlen(namedPipe) + strlen(IISNODE_VERSION) + 6 + 17), ERROR_NOT_ENOUGH_MEMORY);
	sprintf(tmpIndex, "PORT=%s", namedPipe);
	tmpIndex += strlen(namedPipe) + 6;
	sprintf(tmpIndex, "IISNODE_VERSION=%s", IISNODE_VERSION);
	tmpIndex += strlen(IISNODE_VERSION) + 17;

	// set DEBUGPORT environment variable if requested (used by node-inspector)

	if (debugPort > 0)
	{
		char debugPortS[64];
		sprintf(debugPortS, "%d", debugPort);
		ErrorIf((tmpSize - (tmpIndex - tmpStart)) < (strlen(debugPortS) + 11), ERROR_NOT_ENOUGH_MEMORY);
		sprintf(tmpIndex, "DEBUGPORT=%s", debugPortS);
		tmpIndex += strlen(debugPortS) + 11;
	}
		
	// add environment variables from the appSettings section of config

	ErrorIf(NULL == (keyPropertyName = SysAllocString(L"key")), ERROR_NOT_ENOUGH_MEMORY);
	vKeyPropertyName.vt = VT_BSTR;
	vKeyPropertyName.bstrVal = keyPropertyName;
	ErrorIf(NULL == (valuePropertyName = SysAllocString(L"value")), ERROR_NOT_ENOUGH_MEMORY);
	vValuePropertyName.vt = VT_BSTR;
	vValuePropertyName.bstrVal = valuePropertyName;
	CheckError(GetConfigSection(ctx, &section, L"appSettings"));
	CheckError(section->get_Collection(&appSettings));
	CheckError(appSettings->get_Count(&count));

	for (USHORT i = 0; i < count; i++)
	{
		VARIANT index;
		index.vt = VT_I2;
		index.iVal = i;		

		CheckError(appSettings->get_Item(index, &entry));
		CheckError(entry->get_Properties(&properties));
		
		CheckError(properties->get_Item(vKeyPropertyName, &prop));
		CheckError(prop->get_StringValue(&propertyValue));
		if (L'\0' != *propertyValue) 
		{
			// the name of the setting is non-empty

			ErrorIf(0 == (propertySize = WideCharToMultiByte(CP_ACP, 0, propertyValue, wcslen(propertyValue), NULL, 0, NULL, NULL)), E_FAIL);
			ErrorIf((propertySize + 2) > (tmpSize - (tmpStart - tmpIndex)), ERROR_NOT_ENOUGH_MEMORY);
			ErrorIf(propertySize != WideCharToMultiByte(CP_ACP, 0, propertyValue, wcslen(propertyValue), tmpIndex, propertySize, NULL, NULL), E_FAIL);
			tmpIndex[propertySize] = '=';
			tmpIndex += propertySize + 1;
			SysFreeString(propertyValue);
			propertyValue = NULL;
			prop->Release();
			prop = NULL;

			CheckError(properties->get_Item(vValuePropertyName, &prop));
			CheckError(prop->get_StringValue(&propertyValue));
			if (L'\0' != *propertyValue)
			{
				// the value of the property is non-empty

				ErrorIf(0 == (propertySize = WideCharToMultiByte(CP_ACP, 0, propertyValue, wcslen(propertyValue), NULL, 0, NULL, NULL)), E_FAIL);
				ErrorIf((propertySize + 1) > (tmpSize - (tmpStart - tmpIndex)), ERROR_NOT_ENOUGH_MEMORY);
				ErrorIf(propertySize != WideCharToMultiByte(CP_ACP, 0, propertyValue, wcslen(propertyValue), tmpIndex, propertySize, NULL, NULL), E_FAIL);
				tmpIndex += propertySize + 1;
			}
			else
			{
				// zero-terminate the environment variable of the form "foo=" (i.e. without a value)

				*tmpIndex = L'\0';
				tmpIndex++;
			}
		}

		SysFreeString(propertyValue);
		propertyValue = NULL;
		prop->Release();
		prop = NULL;

		properties->Release();
		properties = NULL;
		entry->Release();
		entry = NULL;
	}

	// set NODE_ENV variable based on the iisnode/@node_env configuration setting, if not empty

	LPWSTR node_env = CModuleConfiguration::GetNodeEnv(ctx);
	if (0 != wcscmp(L"", node_env) && 0 != wcscmp(L"%node_env%", node_env))
	{
		ErrorIf((tmpSize - (tmpIndex - tmpStart)) < 10, ERROR_NOT_ENOUGH_MEMORY);
		sprintf(tmpIndex, "NODE_ENV=");
		tmpIndex += 9;
		ErrorIf(0 == (propertySize = WideCharToMultiByte(CP_ACP, 0, node_env, wcslen(node_env), NULL, 0, NULL, NULL)), E_FAIL);
		ErrorIf((propertySize + 1) > (tmpSize - (tmpStart - tmpIndex)), ERROR_NOT_ENOUGH_MEMORY);
		ErrorIf(propertySize != WideCharToMultiByte(CP_ACP, 0, node_env, wcslen(node_env), tmpIndex, propertySize, NULL, NULL), E_FAIL);
		tmpIndex += propertySize + 1;
	}

	// add a trailing zero to new variables

	ErrorIf(1 > (tmpSize - (tmpStart - tmpIndex)), ERROR_NOT_ENOUGH_MEMORY);
	*tmpIndex = 0;
	tmpIndex++;

	// concatenate new environment variables with the current environment block

	ErrorIf(NULL == (*env = (LPCH)new char[environmentSize + (tmpIndex - tmpStart)]), ERROR_NOT_ENOUGH_MEMORY);	
	memcpy(*env, currentEnvironment, environmentSize);
	memcpy(*env + environmentSize - 1, tmpStart, (tmpIndex - tmpStart));

	// cleanup

	FreeEnvironmentStrings(currentEnvironment);
	section->Release();
	appSettings->Release();
	SysFreeString(keyPropertyName);
	SysFreeString(valuePropertyName);
	delete [] tmpStart;

	return S_OK;
Error:

	if (currentEnvironment)
	{
		FreeEnvironmentStrings(currentEnvironment);
		currentEnvironment = NULL;
	}

	if (section)
	{
		section->Release();
		section = NULL;
	}

	if (appSettings)
	{
		appSettings->Release();
		appSettings = NULL;
	}

	if (keyPropertyName)
	{
		SysFreeString(keyPropertyName);
		keyPropertyName = NULL;
	}

	if (valuePropertyName)
	{
		SysFreeString(valuePropertyName);
		valuePropertyName = NULL;
	}

	if (entry)
	{
		entry->Release();
		entry = NULL;
	}

	if (properties)
	{
		properties->Release();
		properties = NULL;
	}

	if (prop)
	{
		prop->Release();
		prop = NULL;
	}

	if (propertyValue)
	{
		SysFreeString(propertyValue);
		propertyValue = NULL;
	}

	if (tmpStart)
	{
		delete [] tmpStart;
		tmpStart = NULL;
	}

	return hr;
}

HRESULT CModuleConfiguration::GetConfigSection(IHttpContext* context, IAppHostElement** section, OLECHAR* configElement)
{
	HRESULT hr = S_OK;
    BSTR path = NULL;
    BSTR elementName = NULL;

	CheckNull(section);
	*section = NULL;
	ErrorIf(NULL == (path = SysAllocString(context->GetMetadata()->GetMetaPath())), ERROR_NOT_ENOUGH_MEMORY);
    ErrorIf(NULL == (elementName = SysAllocString(configElement)), ERROR_NOT_ENOUGH_MEMORY);
	CheckError(server->GetAdminManager()->GetAdminSection(elementName, path, section));

Error:

    if (NULL != elementName)
    {
        SysFreeString(elementName);
        elementName = NULL;
    }

    if (NULL != path)
    {
        SysFreeString(path);
        path = NULL;
    }

    return hr;
}

HRESULT CModuleConfiguration::GetString(IAppHostElement* section, LPCWSTR propertyName, LPWSTR* value)
{
	HRESULT hr = S_OK;
    BSTR sysPropertyName = NULL;
    BSTR sysPropertyValue = NULL;
    IAppHostProperty* prop = NULL;

	CheckNull(value);
	*value = NULL;
	ErrorIf(NULL == (sysPropertyName = SysAllocString(propertyName)), ERROR_NOT_ENOUGH_MEMORY);
	CheckError(section->GetPropertyByName(sysPropertyName, &prop));
	CheckError(prop->get_StringValue(&sysPropertyValue));
	ErrorIf(NULL == (*value = new WCHAR[wcslen(sysPropertyValue) + 1]), ERROR_NOT_ENOUGH_MEMORY);
	wcscpy(*value, sysPropertyValue);

Error:

    if ( sysPropertyName )
    {
        SysFreeString(sysPropertyName);
        sysPropertyName = NULL;
    }

    if ( sysPropertyValue )
    {
        SysFreeString(sysPropertyValue);
        sysPropertyValue = NULL;
    }

    if (prop)
    {
        prop->Release();
        prop = NULL;
    }

    return hr;
}

HRESULT CModuleConfiguration::GetBOOL(IAppHostElement* section, LPCWSTR propertyName, BOOL* value)
{
	HRESULT hr = S_OK;
    BSTR sysPropertyName = NULL;
    IAppHostProperty* prop = NULL;
	VARIANT var;

	CheckNull(value);
	*value = FALSE;
	VariantInit(&var);
	ErrorIf(NULL == (sysPropertyName = SysAllocString(propertyName)), ERROR_NOT_ENOUGH_MEMORY);
	CheckError(section->GetPropertyByName(sysPropertyName, &prop));
	CheckError(prop->get_Value(&var));
	CheckError(VariantChangeType(&var, &var, 0, VT_BOOL));
	*value = (V_BOOL(&var) == VARIANT_TRUE);

Error:

	VariantClear(&var);

    if ( sysPropertyName )
    {
        SysFreeString(sysPropertyName);
        sysPropertyName = NULL;
    }

    if (prop)
    {
        prop->Release();
        prop = NULL;
    }

    return hr;
}

HRESULT CModuleConfiguration::GetDWORD(IAppHostElement* section, LPCWSTR propertyName, DWORD* value)
{
	HRESULT hr = S_OK;
    BSTR sysPropertyName = NULL;
    IAppHostProperty* prop = NULL;
	VARIANT var;

	CheckNull(value);
	*value = 0;
	VariantInit(&var);
	ErrorIf(NULL == (sysPropertyName = SysAllocString(propertyName)), ERROR_NOT_ENOUGH_MEMORY);
	CheckError(section->GetPropertyByName(sysPropertyName, &prop));
	CheckError(prop->get_Value(&var));
	CheckError(VariantChangeType(&var, &var, 0, VT_UI4));
	*value = var.ulVal;

Error:

	VariantClear(&var);

    if ( sysPropertyName )
    {
        SysFreeString(sysPropertyName);
        sysPropertyName = NULL;
    }

    if (prop)
    {
        prop->Release();
        prop = NULL;
    }

    return hr;
}

HRESULT CModuleConfiguration::GetConfig(IHttpContext* context, CModuleConfiguration** config)
{
	HRESULT hr;
	CModuleConfiguration* c = NULL;
	IAppHostElement* section = NULL;
	LPWSTR commandLine = NULL;
	size_t i;

	CheckNull(config);

	*config = (CModuleConfiguration*)context->GetMetadata()->GetModuleContextContainer()->GetModuleContext(moduleId);

	if (NULL == *config)
	{
		ErrorIf(NULL == (c = new CModuleConfiguration()), ERROR_NOT_ENOUGH_MEMORY);
		
		CheckError(GetConfigSection(context, &section));		
		CheckError(GetDWORD(section, L"asyncCompletionThreadCount", &c->asyncCompletionThreadCount));
		CheckError(GetDWORD(section, L"nodeProcessCountPerApplication", &c->nodeProcessCountPerApplication));
		CheckError(GetDWORD(section, L"maxConcurrentRequestsPerProcess", &c->maxConcurrentRequestsPerProcess));
		CheckError(GetDWORD(section, L"maxNamedPipeConnectionRetry", &c->maxNamedPipeConnectionRetry));
		CheckError(GetDWORD(section, L"namedPipeConnectionRetryDelay", &c->namedPipeConnectionRetryDelay));
		CheckError(GetDWORD(section, L"initialRequestBufferSize", &c->initialRequestBufferSize));
		CheckError(GetDWORD(section, L"maxRequestBufferSize", &c->maxRequestBufferSize));
		CheckError(GetDWORD(section, L"uncFileChangesPollingInterval", &c->uncFileChangesPollingInterval));
		CheckError(GetDWORD(section, L"gracefulShutdownTimeout", &c->gracefulShutdownTimeout));
		CheckError(GetDWORD(section, L"logFileFlushInterval", &c->logFileFlushInterval));
		CheckError(GetDWORD(section, L"maxLogFileSizeInKB", &c->maxLogFileSizeInKB));
		CheckError(GetBOOL(section, L"loggingEnabled", &c->loggingEnabled));
		CheckError(GetBOOL(section, L"appendToExistingLog", &c->appendToExistingLog));
		CheckError(GetBOOL(section, L"devErrorsEnabled", &c->devErrorsEnabled));
		CheckError(GetBOOL(section, L"flushResponse", &c->flushResponse));
		CheckError(GetString(section, L"logDirectoryNameSuffix", &c->logDirectoryNameSuffix));
		CheckError(GetBOOL(section, L"debuggingEnabled", &c->debuggingEnabled));
		CheckError(GetString(section, L"node_env", &c->node_env));
		CheckError(GetString(section, L"debuggerPortRange", &c->debugPortRange));
		CheckError(GetString(section, L"debuggerPathSegment", &c->debuggerPathSegment));
		c->debuggerPathSegmentLength = wcslen(c->debuggerPathSegment);
		CheckError(GetString(section, L"nodeProcessCommandLine", &commandLine));
		ErrorIf(NULL == (c->nodeProcessCommandLine = new char[MAX_PATH]), ERROR_NOT_ENOUGH_MEMORY);
		ErrorIf(0 != wcstombs_s(&i, c->nodeProcessCommandLine, (size_t)MAX_PATH, commandLine, _TRUNCATE), ERROR_INVALID_PARAMETER);
		delete [] commandLine;
		commandLine = NULL;
		
		section->Release();
		section = NULL;

        // apply defaults

        if (0 == c->asyncCompletionThreadCount)
        {
            // default number of async completion threads is the number of processors

            SYSTEM_INFO info;

            GetSystemInfo(&info);
            c->asyncCompletionThreadCount = 0 == info.dwNumberOfProcessors ? 4 : info.dwNumberOfProcessors;
        }

		if (0 == c->nodeProcessCountPerApplication)
		{
			// default number of node.exe processes to create per node.js application

            SYSTEM_INFO info;

            GetSystemInfo(&info);
            c->nodeProcessCountPerApplication = 0 == info.dwNumberOfProcessors ? 1 : info.dwNumberOfProcessors;
		}
		
		// CR: check for ERROR_ALREADY_ASSIGNED to detect a race in creation of this section 
		// CR: refcounting may be needed if synchronous code paths proove too long (race with config changes)
		context->GetMetadata()->GetModuleContextContainer()->SetModuleContext(c, moduleId);
		*config = c;
		c = NULL;
	}

	return S_OK;
Error:

	if (NULL != section)
	{
		section->Release();
		section = NULL;
	}

	if (NULL != commandLine)
	{
		delete [] commandLine;
		commandLine = NULL;
	}

	if (NULL != c)
	{
		delete c;
		c = NULL;
	}

	return hr;
}

void CModuleConfiguration::CleanupStoredContext()
{
	delete this;
}

#define GETCONFIG(prop) \
	CModuleConfiguration* c; \
	GetConfig(ctx, &c); \
	return c->prop;

DWORD CModuleConfiguration::GetAsyncCompletionThreadCount(IHttpContext* ctx)
{
	GETCONFIG(asyncCompletionThreadCount)
}

DWORD CModuleConfiguration::GetNodeProcessCountPerApplication(IHttpContext* ctx)
{
	GETCONFIG(nodeProcessCountPerApplication)
}

LPCTSTR CModuleConfiguration::GetNodeProcessCommandLine(IHttpContext* ctx)
{
	GETCONFIG(nodeProcessCommandLine)
}

DWORD CModuleConfiguration::GetMaxConcurrentRequestsPerProcess(IHttpContext* ctx)
{
	GETCONFIG(maxConcurrentRequestsPerProcess)
}

DWORD CModuleConfiguration::GetMaxNamedPipeConnectionRetry(IHttpContext* ctx)
{
	GETCONFIG(maxNamedPipeConnectionRetry)
}

DWORD CModuleConfiguration::GetNamedPipeConnectionRetryDelay(IHttpContext* ctx)
{
	GETCONFIG(namedPipeConnectionRetryDelay)
}

DWORD CModuleConfiguration::GetInitialRequestBufferSize(IHttpContext* ctx)
{
	GETCONFIG(initialRequestBufferSize)
}

DWORD CModuleConfiguration::GetMaxRequestBufferSize(IHttpContext* ctx)
{
	GETCONFIG(maxRequestBufferSize)
}

DWORD CModuleConfiguration::GetUNCFileChangesPollingInterval(IHttpContext* ctx)
{
	GETCONFIG(uncFileChangesPollingInterval)
}

DWORD CModuleConfiguration::GetGracefulShutdownTimeout(IHttpContext* ctx)
{
	GETCONFIG(gracefulShutdownTimeout)
}

LPWSTR CModuleConfiguration::GetLogDirectoryNameSuffix(IHttpContext* ctx)
{
	GETCONFIG(logDirectoryNameSuffix)
}

LPWSTR CModuleConfiguration::GetDebuggerPathSegment(IHttpContext* ctx)
{
	GETCONFIG(debuggerPathSegment)
}

DWORD CModuleConfiguration::GetDebuggerPathSegmentLength(IHttpContext* ctx)
{
	GETCONFIG(debuggerPathSegmentLength)
}

DWORD CModuleConfiguration::GetLogFileFlushInterval(IHttpContext* ctx)
{
	GETCONFIG(logFileFlushInterval)
}

DWORD CModuleConfiguration::GetMaxLogFileSizeInKB(IHttpContext* ctx)
{
	GETCONFIG(maxLogFileSizeInKB)
}

BOOL CModuleConfiguration::GetLoggingEnabled(IHttpContext* ctx)
{
	GETCONFIG(loggingEnabled);
}

BOOL CModuleConfiguration::GetAppendToExistingLog(IHttpContext* ctx)
{
	GETCONFIG(appendToExistingLog)
}

BOOL CModuleConfiguration::GetDebuggingEnabled(IHttpContext* ctx)
{
	GETCONFIG(debuggingEnabled)
}

LPWSTR CModuleConfiguration::GetNodeEnv(IHttpContext* ctx)
{
	GETCONFIG(node_env)
}

BOOL CModuleConfiguration::GetDevErrorsEnabled(IHttpContext* ctx)
{
	GETCONFIG(devErrorsEnabled)
}

BOOL CModuleConfiguration::GetFlushResponse(IHttpContext* ctx)
{
	GETCONFIG(flushResponse)
}

HRESULT CModuleConfiguration::GetDebugPortRange(IHttpContext* ctx, DWORD* start, DWORD* end)
{
	HRESULT hr;

	CheckNull(start);
	CheckNull(end);

	CModuleConfiguration* c = NULL; 
	GetConfig(ctx, &c); 

	if (0 == c->debugPortStart)
	{
		CheckNull(c->debugPortRange);
		LPWSTR dash = c->debugPortRange;
		while (*dash != L'\0' && *dash != L'-')
			dash++;
		ErrorIf(dash == c->debugPortRange, ERROR_INVALID_PARAMETER);
		c->debugPortStart = _wtoi(c->debugPortRange);
		ErrorIf(c->debugPortStart < 1025, ERROR_INVALID_PARAMETER);
		if (*dash != L'\0')
		{
			c->debugPortEnd = _wtoi(dash + 1);
			ErrorIf(c->debugPortEnd < c->debugPortStart || c->debugPortEnd > 65535, ERROR_INVALID_PARAMETER);
		}
		else
		{
			c->debugPortEnd = c->debugPortStart;
		}
	}

	*start = c->debugPortStart;
	*end = c->debugPortEnd;

	return S_OK;
Error:

	if (c)
	{
		c->debugPortStart = c->debugPortEnd = 0;
	}

	return hr;
}
