#include "precomp.h"

IHttpServer* CModuleConfiguration::server = NULL;

HTTP_MODULE_ID CModuleConfiguration::moduleId = NULL;

CModuleConfiguration::CModuleConfiguration()
	: nodeProcessCommandLine(NULL), logDirectoryNameSuffix(NULL)
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

HRESULT CModuleConfiguration::CreateNodeEnvironment(IHttpContext* ctx, PCH namedPipe, PCH* env)
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
	BSTR propertyValue;
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
		ErrorIf(0 == (propertySize = WideCharToMultiByte(CP_ACP, 0, propertyValue, wcslen(propertyValue), NULL, 0, NULL, NULL)), E_FAIL);
		ErrorIf((propertySize + 1) > (tmpSize - (tmpStart - tmpIndex)), ERROR_NOT_ENOUGH_MEMORY);
		ErrorIf(propertySize != WideCharToMultiByte(CP_ACP, 0, propertyValue, wcslen(propertyValue), tmpIndex, propertySize, NULL, NULL), E_FAIL);
		tmpIndex += propertySize + 1;
		SysFreeString(propertyValue);
		propertyValue = NULL;
		prop->Release();
		prop = NULL;

		properties->Release();
		properties = NULL;
		entry->Release();
		entry = NULL;
	}

	// concatenate new environment variables with the current environment block

	ErrorIf(NULL == (*env = (LPCH)new char[environmentSize + (tmpIndex - tmpStart)]), ERROR_NOT_ENOUGH_MEMORY);	
	memcpy(*env, tmpStart, (tmpIndex - tmpStart));
	memcpy(*env + (tmpIndex - tmpStart), currentEnvironment, environmentSize);

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
		CheckError(GetDWORD(section, L"maxPendingRequestsPerApplication", &c->maxPendingRequestsPerApplication));
		CheckError(GetDWORD(section, L"asyncCompletionThreadCount", &c->asyncCompletionThreadCount));
		CheckError(GetDWORD(section, L"maxProcessCountPerApplication", &c->maxProcessCountPerApplication));
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
		CheckError(GetString(section, L"logDirectoryNameSuffix", &c->logDirectoryNameSuffix));
		CheckError(GetString(section, L"nodeProcessCommandLine", &commandLine));
		ErrorIf(NULL == (c->nodeProcessCommandLine = new char[MAX_PATH]), ERROR_NOT_ENOUGH_MEMORY);
		ErrorIf(0 != wcstombs_s(&i, c->nodeProcessCommandLine, (size_t)MAX_PATH, commandLine, _TRUNCATE), ERROR_INVALID_PARAMETER);
		delete [] commandLine;
		commandLine = NULL;
		
		section->Release();
		section = NULL;
		
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

DWORD CModuleConfiguration::GetMaxPendingRequestsPerApplication(IHttpContext* ctx)
{
	GETCONFIG(maxPendingRequestsPerApplication)
}

DWORD CModuleConfiguration::GetAsyncCompletionThreadCount(IHttpContext* ctx)
{
	GETCONFIG(asyncCompletionThreadCount)
}

DWORD CModuleConfiguration::GetMaxProcessCountPerApplication(IHttpContext* ctx)
{
	GETCONFIG(maxProcessCountPerApplication)
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
	GETCONFIG(loggingEnabled)
}

BOOL CModuleConfiguration::GetAppendToExistingLog(IHttpContext* ctx)
{
	GETCONFIG(appendToExistingLog)
}
