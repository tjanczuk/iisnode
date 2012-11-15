#include "precomp.h"

IHttpServer* CModuleConfiguration::server = NULL;

HTTP_MODULE_ID CModuleConfiguration::moduleId = NULL;

BOOL CModuleConfiguration::invalid = FALSE;

CModuleConfiguration::CModuleConfiguration()
	: nodeProcessCommandLine(NULL), nodeProcessWorkingDirectory(NULL), logDirectory(NULL), debuggerPathSegment(NULL), 
	debugPortRange(NULL), debugPortStart(0), debugPortEnd(0), node_env(NULL), watchedFiles(NULL),
	enableXFF(FALSE), promoteServerVars(NULL), promoteServerVarsRaw(NULL), configOverridesFileName(NULL),
	configOverrides(NULL), interceptor(NULL), debugHeaderEnabled(FALSE)
{
	InitializeSRWLock(&this->srwlock);
}

CModuleConfiguration::~CModuleConfiguration()
{	
	if (NULL != this->nodeProcessCommandLine)
	{
		delete [] this->nodeProcessCommandLine;
		this->nodeProcessCommandLine = NULL;
	}

	if (NULL != this->nodeProcessWorkingDirectory)
	{
		delete [] this->nodeProcessWorkingDirectory;
		this->nodeProcessWorkingDirectory = NULL;
	}

	if (NULL != this->interceptor) {
		delete [] this->interceptor;
		this->interceptor = NULL;
	}

	if (NULL != this->logDirectory)
	{
		delete [] this->logDirectory;
		this->logDirectory = NULL;
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

	if (NULL != this->watchedFiles)
	{
		delete this->watchedFiles;
		this->watchedFiles = NULL;
	}

	if (NULL != this->promoteServerVars)
	{
		for (int i = 0; i < this->promoteServerVarsCount; i++)
		{
			if (this->promoteServerVars[i])
			{
				delete [] this->promoteServerVars[i];
			}
		}

		delete [] this->promoteServerVars;
		this->promoteServerVars = NULL;
	}

	if (NULL != this->promoteServerVarsRaw)
	{
		delete [] this->promoteServerVarsRaw;
		this->promoteServerVarsRaw = NULL;
	}

	if (NULL != this->configOverridesFileName)
	{
		delete [] this->configOverridesFileName;
		this->configOverridesFileName = NULL;
	}

	if (NULL != this->configOverrides)
	{
		delete [] this->configOverrides;
		this->configOverrides = NULL;
	}
}

void CModuleConfiguration::Invalidate()
{
	CModuleConfiguration::invalid = TRUE;
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

	if (CModuleConfiguration::GetLoggingEnabled(ctx)) 
	{
		// set IISNODE_LOGGINGENABLED variable	

		ErrorIf((tmpSize - (tmpIndex - tmpStart)) < 25, ERROR_NOT_ENOUGH_MEMORY);
		sprintf(tmpIndex, "IISNODE_LOGGINGENABLED=1");
		tmpIndex += 25;

		// set IISNODE_LOGDIRECTORY variable based on the iisnode/@logDirectory configuration setting, if not empty

		LPWSTR logDirectory = CModuleConfiguration::GetLogDirectory(ctx);
		if (0 != wcscmp(L"", logDirectory))
		{
			ErrorIf((tmpSize - (tmpIndex - tmpStart)) < 22, ERROR_NOT_ENOUGH_MEMORY);
			sprintf(tmpIndex, "IISNODE_LOGDIRECTORY=");
			tmpIndex += 21;
			ErrorIf(0 == (propertySize = WideCharToMultiByte(CP_ACP, 0, logDirectory, wcslen(logDirectory), NULL, 0, NULL, NULL)), E_FAIL);
			ErrorIf((propertySize + 1) > (tmpSize - (tmpStart - tmpIndex)), ERROR_NOT_ENOUGH_MEMORY);
			ErrorIf(propertySize != WideCharToMultiByte(CP_ACP, 0, logDirectory, wcslen(logDirectory), tmpIndex, propertySize, NULL, NULL), E_FAIL);
			tmpIndex += propertySize + 1;
		}

		// set IISNODE_MAXTOTALLOGFILESIZEINKB, IISNODE_MAXLOGFILESIZEINKB, and IISNODE_MAXLOGFILES variables

		char tmp[64];
		sprintf(tmp, "%d", CModuleConfiguration::GetMaxTotalLogFileSizeInKB(ctx));
		ErrorIf((tmpSize - (tmpIndex - tmpStart)) < (strlen(tmp) + 33), ERROR_NOT_ENOUGH_MEMORY);
		sprintf(tmpIndex, "IISNODE_MAXTOTALLOGFILESIZEINKB=%s", tmp);
		tmpIndex += strlen(tmp) + 33;

		sprintf(tmp, "%d", CModuleConfiguration::GetMaxLogFileSizeInKB(ctx));
		ErrorIf((tmpSize - (tmpIndex - tmpStart)) < (strlen(tmp) + 28), ERROR_NOT_ENOUGH_MEMORY);
		sprintf(tmpIndex, "IISNODE_MAXLOGFILESIZEINKB=%s", tmp);
		tmpIndex += strlen(tmp) + 28;

		sprintf(tmp, "%d", CModuleConfiguration::GetMaxLogFiles(ctx));
		ErrorIf((tmpSize - (tmpIndex - tmpStart)) < (strlen(tmp) + 21), ERROR_NOT_ENOUGH_MEMORY);
		sprintf(tmpIndex, "IISNODE_MAXLOGFILES=%s", tmp);
		tmpIndex += strlen(tmp) + 21;

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

HRESULT CModuleConfiguration::GetBOOL(IAppHostElement* section, LPCWSTR propertyName, BOOL* value, BOOL defaultValue)
{
	HRESULT hr = S_OK;
    BSTR sysPropertyName = NULL;
    IAppHostProperty* prop = NULL;
	VARIANT var;

	CheckNull(value);
	*value = FALSE;
	VariantInit(&var);
	ErrorIf(NULL == (sysPropertyName = SysAllocString(propertyName)), ERROR_NOT_ENOUGH_MEMORY);
	if (S_OK != section->GetPropertyByName(sysPropertyName, &prop)) 
	{
		*value = defaultValue;
	}
	else 
	{
		CheckError(prop->get_Value(&var));
		CheckError(VariantChangeType(&var, &var, 0, VT_BOOL));
		*value = (V_BOOL(&var) == VARIANT_TRUE);
	}

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

HRESULT CModuleConfiguration::GetDWORD(char* str, DWORD* value)
{
	HRESULT hr;

	if (str)
	{
		long v = atol(str);
		ErrorIf((v == LONG_MAX || v == LONG_MIN) && errno == ERANGE, E_FAIL);
		*value = (DWORD)v;
	}

	return S_OK;
Error:
	return hr;
}

HRESULT CModuleConfiguration::GetBOOL(char* str, BOOL* value)
{
	if (!str)
	{
		*value = FALSE;
	}
	else if (0 == strcmpi(str, "false") || 0 == strcmpi(str, "0") || 0 == strcmpi(str, "no"))
	{
		*value = FALSE;
	}
	else if (0 == strcmpi(str, "true") || 0 == strcmpi(str, "1") || 0 == strcmpi(str, "yes"))
	{
		*value = TRUE;
	}
	else
	{
		return E_FAIL;
	}

	return S_OK;
}

HRESULT CModuleConfiguration::GetString(char* str, LPWSTR* value)
{
	HRESULT hr;
	int wcharSize, bytesConverted;

	if (*value)
	{
		delete [] *value;
		*value = NULL;
	}

	if (!str)
	{
		str = "";
	}

	ErrorIf(0 == (wcharSize = MultiByteToWideChar(CP_ACP, 0, str, -1, NULL, 0)), GetLastError());
	ErrorIf(NULL == (*value = new WCHAR[wcharSize]), ERROR_NOT_ENOUGH_MEMORY);
	ErrorIf(wcharSize != MultiByteToWideChar(CP_ACP, 0, str, -1, *value, wcharSize), GetLastError());

	return S_OK;
Error:
	return hr;
}

HRESULT CModuleConfiguration::ApplyConfigOverrideKeyValue(IHttpContext* context, CModuleConfiguration* config, char* keyStart, char* keyEnd, char* valueStart, char* valueEnd)
{
	HRESULT hr;

	keyEnd++;
	*keyEnd = 0;

	if (valueEnd)
	{
		valueEnd++;
		*valueEnd = 0;
	}

	if (0 == strcmpi(keyStart, "asyncCompletionThreadCount"))
	{
		CheckError(GetDWORD(valueStart, &config->asyncCompletionThreadCount));
	}
	else if (0 == strcmpi(keyStart, "nodeProcessCountPerApplication"))
	{
		CheckError(GetDWORD(valueStart, &config->nodeProcessCountPerApplication));
	}
	else if (0 == strcmpi(keyStart, "maxConcurrentRequestsPerProcess"))
	{
		CheckError(GetDWORD(valueStart, &config->maxConcurrentRequestsPerProcess));
	}
	else if (0 == strcmpi(keyStart, "maxNamedPipeConnectionRetry"))
	{
		CheckError(GetDWORD(valueStart, &config->maxNamedPipeConnectionRetry));
	}
	else if (0 == strcmpi(keyStart, "namedPipeConnectionRetryDelay"))
	{
		CheckError(GetDWORD(valueStart, &config->namedPipeConnectionRetryDelay));
	}
	else if (0 == strcmpi(keyStart, "maxNamedPipeConnectionPoolSize"))
	{
		CheckError(GetDWORD(valueStart, &config->maxNamedPipeConnectionPoolSize));
	}
	else if (0 == strcmpi(keyStart, "maxNamedPipePooledConnectionAge"))
	{
		CheckError(GetDWORD(valueStart, &config->maxNamedPipePooledConnectionAge));
	}
	else if (0 == strcmpi(keyStart, "initialRequestBufferSize"))
	{
		CheckError(GetDWORD(valueStart, &config->initialRequestBufferSize));
	}
	else if (0 == strcmpi(keyStart, "maxRequestBufferSize"))
	{
		CheckError(GetDWORD(valueStart, &config->maxRequestBufferSize));
	}
	else if (0 == strcmpi(keyStart, "uncFileChangesPollingInterval"))
	{
		CheckError(GetDWORD(valueStart, &config->uncFileChangesPollingInterval));
	}
	else if (0 == strcmpi(keyStart, "gracefulShutdownTimeout"))
	{
		CheckError(GetDWORD(valueStart, &config->gracefulShutdownTimeout));
	}
	else if (0 == strcmpi(keyStart, "maxTotalLogFileSizeInKB"))
	{
		CheckError(GetDWORD(valueStart, &config->maxTotalLogFileSizeInKB));
	}
	else if (0 == strcmpi(keyStart, "maxLogFileSizeInKB"))
	{
		CheckError(GetDWORD(valueStart, &config->maxLogFileSizeInKB));
	}
	else if (0 == strcmpi(keyStart, "maxLogFiles"))
	{
		CheckError(GetDWORD(valueStart, &config->maxLogFiles));
	}
	else if (0 == strcmpi(keyStart, "loggingEnabled"))
	{
		CheckError(GetBOOL(valueStart, &config->loggingEnabled));
	}
	else if (0 == strcmpi(keyStart, "devErrorsEnabled"))
	{
		CheckError(GetBOOL(valueStart, &config->devErrorsEnabled));
	}
	else if (0 == strcmpi(keyStart, "flushResponse"))
	{
		CheckError(GetBOOL(valueStart, &config->flushResponse));
	}
	else if (0 == strcmpi(keyStart, "debuggingEnabled"))
	{
		CheckError(GetBOOL(valueStart, &config->debuggingEnabled));
	}
	else if (0 == strcmpi(keyStart, "debugHeaderEnabled"))
	{
		CheckError(GetBOOL(valueStart, &config->debugHeaderEnabled));
	}
	else if (0 == strcmpi(keyStart, "enableXFF"))
	{
		CheckError(GetBOOL(valueStart, &config->enableXFF));
	}
	else if (0 == strcmpi(keyStart, "logDirectory"))
	{
		CheckError(GetString(valueStart, &config->logDirectory));
	}
	else if (0 == strcmpi(keyStart, "node_env"))
	{
		CheckError(GetString(valueStart, &config->node_env));
	}
	else if (0 == strcmpi(keyStart, "debugPortRange"))
	{
		CheckError(GetString(valueStart, &config->debugPortRange));
	}
	else if (0 == strcmpi(keyStart, "watchedFiles"))
	{
		CheckError(GetString(valueStart, &config->watchedFiles));
	}
	else if (0 == strcmpi(keyStart, "promoteServerVars"))
	{
		CheckError(GetString(valueStart, &config->promoteServerVarsRaw));
	}
	else if (0 == strcmpi(keyStart, "debuggerPathSegment"))
	{
		CheckError(GetString(valueStart, &config->debuggerPathSegment));
		config->debuggerPathSegmentLength = wcslen(config->debuggerPathSegment);
	}
	else if (0 == strcmpi(keyStart, "nodeProcessCommandLine"))
	{
		CheckError(GetString(valueStart, &config->nodeProcessCommandLine));
	}
	else if (0 == strcmpi(keyStart, "nodeProcessWorkingDirectory"))
	{
		CheckError(GetString(valueStart, &config->nodeProcessWorkingDirectory));
	}
	else if (0 == strcmpi(keyStart, "interceptor"))
	{
		CheckError(GetString(valueStart, &config->interceptor));
	}

	return S_OK;
Error:
	return hr;
}

HRESULT CModuleConfiguration::ApplyYamlConfigOverrides(IHttpContext* context, CModuleConfiguration* config) 
{
	HRESULT hr;
	PCWSTR scriptTranslated;
	DWORD scriptTranslatedLength;
	HANDLE overridesHandle = INVALID_HANDLE_VALUE;
	DWORD fileSize;
	DWORD bytesRead;
	char* content = NULL;
	char* lineStart;
	char* lineEnd;
	char* colon;
	char* comment;
	char *keyStart, *keyEnd;
	char *valueStart, *valueEnd;

	if (config->configOverrides == L'\0')
	{
		// no file name with config overrides specified, return success

		return S_OK;
	}

	// construct absolute file name by replacing the script name in the script translated path with the config override file name

	if (!config->configOverridesFileName)
	{
		scriptTranslated = context->GetScriptTranslated(&scriptTranslatedLength);
		ErrorIf(NULL == (config->configOverridesFileName = new WCHAR[scriptTranslatedLength + wcslen(config->configOverrides) + 1]), ERROR_NOT_ENOUGH_MEMORY);
		wcscpy(config->configOverridesFileName, scriptTranslated);
		while (scriptTranslatedLength > 0 && config->configOverridesFileName[scriptTranslatedLength] != L'\\')
			scriptTranslatedLength--;
		wcscpy(config->configOverridesFileName + scriptTranslatedLength + 1, config->configOverrides);
	}

	// open configuration override file if it exists

	if (INVALID_HANDLE_VALUE == (overridesHandle = CreateFileW(
		config->configOverridesFileName,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,
		0,
		NULL))) {
			hr = GetLastError();

			// if file does not exist, clean up and return success (i.e. no config overrides specified)
			ErrorIf(ERROR_FILE_NOT_FOUND == hr, S_OK);
			
			// if other error occurred, return the error
			ErrorIf(TRUE, hr);
	}

	// read file content

	ErrorIf(INVALID_FILE_SIZE == (fileSize = GetFileSize(overridesHandle, NULL)), GetLastError());
	ErrorIf(0 == fileSize, S_OK); // empty file, clean up and return success
	ErrorIf(NULL == (content = new char[fileSize + 1]), ERROR_NOT_ENOUGH_MEMORY);
	ErrorIf(!ReadFile(overridesHandle, content, fileSize, &bytesRead, NULL), GetLastError());
	ErrorIf(fileSize != bytesRead, E_FAIL);
	content[bytesRead] = 0;
	CloseHandle(overridesHandle);
	overridesHandle = INVALID_HANDLE_VALUE;

	// parse file

	lineStart = lineEnd = content;
	while (*lineStart) 
	{
		// determine the placement of a comment and colon as well as the end of the line
		colon = comment = NULL;
		while (*lineEnd && *lineEnd != '\r' && *lineEnd != '\n') 
		{
			if (*lineEnd == ':') 
			{
				if (!colon)
					colon = lineEnd;
			}
			else if (*lineEnd == '#')
			{
				if (!comment)
					comment = lineEnd;
			}

			lineEnd++;
		}

		// comment will be the sentinel of the end of this line
		if (!comment)
			comment = lineEnd;

		// skip whitespace at the end of this line and beginning of next
		while (*lineEnd == ' ' || *lineEnd == '\r' || *lineEnd == '\n')
			lineEnd++;

		// skip whitespace at the beginning of line
		while (lineStart < comment && *lineStart == ' ')
			lineStart++;
		
		if (lineStart < comment) 
		{
			// there is a non-whitespace character before the end of the line or comment on that line
			// assume <key>: <value> [#<comment>] syntax of the line
			keyStart = lineStart;
			ErrorIf(!colon, E_FAIL); // there is no colon on the line
			ErrorIf(keyStart == colon, E_FAIL); // colon is the first non-whitespace character on the line

			// find end of key name
			while (lineStart < colon && *lineStart != ' ')
				lineStart++;
			keyEnd = lineStart - 1;

			// skip whitespace between end of key name and colon
			while (lineStart < colon && *lineStart == ' ')
				lineStart++;
			ErrorIf(lineStart != colon, E_FAIL); // non-whitespace character found

			// skip whitespace before value
			lineStart++;
			while (lineStart < comment && *lineStart == ' ')
				lineStart++;

			if (lineStart == comment)
			{
				// empty value
				valueStart = valueEnd = NULL;
			}
			else
			{
				valueStart = lineStart;

				// find end of value as a last non-whitespace character before comment
				valueEnd = comment - 1;
				while (valueEnd > valueStart && *valueEnd == ' ')
					valueEnd--;

				CheckError(CModuleConfiguration::ApplyConfigOverrideKeyValue(context, config, keyStart, keyEnd, valueStart, valueEnd));
			}

		}

		// move on to the next line
		lineStart = lineEnd;
	}

	hr = S_OK; // fall through to cleanup in the Error section
Error:

	if (overridesHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(overridesHandle);
		overridesHandle = INVALID_HANDLE_VALUE;
	}

	if (content)
	{
		delete [] content;
		content = NULL;
	}

	return S_OK == hr ? S_OK : IISNODE_ERROR_UNABLE_TO_READ_CONFIGURATION_OVERRIDE;
}

HRESULT CModuleConfiguration::TokenizePromoteServerVars(CModuleConfiguration* c)
{
	HRESULT hr;
	size_t i;
	size_t varLength;
	LPWSTR start, end;
	wchar_t terminator;	

	if (c->promoteServerVarsRaw)
	{
		if (NULL != c->promoteServerVars)
		{
			for (int i = 0; i < c->promoteServerVarsCount; i++)
			{
				if (c->promoteServerVars[i])
				{
					delete [] c->promoteServerVars[i];
				}
			}

			delete [] c->promoteServerVars;
			c->promoteServerVars = NULL;
		}

		if (*c->promoteServerVarsRaw == L'\0')
		{
			c->promoteServerVarsCount = 0;
		}
		else
		{
			// determine number of server variables

			c->promoteServerVarsCount = 1;
			start = c->promoteServerVarsRaw;
			while (*start) 
			{
				if (L',' == *start)
				{
					c->promoteServerVarsCount++;
				}

				start++;
			}

			// tokenize server variable names (comma delimited list)

			ErrorIf(NULL == (c->promoteServerVars = new char*[c->promoteServerVarsCount]), ERROR_NOT_ENOUGH_MEMORY);
			RtlZeroMemory(c->promoteServerVars, c->promoteServerVarsCount * sizeof(char*));

			i = 0;
			end = c->promoteServerVarsRaw;
			while (*end) 
			{
				start = end;
				while (*end && L',' != *end) 
				{
					end++;
				}

				if (start != end)
				{	
					terminator = *end;
					*end = L'\0';
					ErrorIf(0 != wcstombs_s(&varLength, NULL, 0, start, _TRUNCATE), ERROR_CAN_NOT_COMPLETE);
					ErrorIf(NULL == (c->promoteServerVars[i] = new char[varLength]), ERROR_NOT_ENOUGH_MEMORY);
					ErrorIf(0 != wcstombs_s(&varLength, c->promoteServerVars[i], varLength, start, _TRUNCATE), ERROR_CAN_NOT_COMPLETE);
					i++;
					*end = terminator;
				}

				if (*end)
				{
					end++;
				}
			}
		}

		delete [] c->promoteServerVarsRaw;
		c->promoteServerVarsRaw = NULL;
	}

	return S_OK;
Error:
	return hr;
}

HRESULT CModuleConfiguration::ApplyDefaults(CModuleConfiguration* c)
{
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

	return S_OK;
}

HRESULT CModuleConfiguration::EnsureCurrent(IHttpContext* context, CModuleConfiguration* config)
{
	HRESULT hr;

	if (config && CModuleConfiguration::invalid)
	{
		// yaml config has changed and needs to be re-read; this condition was set by the CFileWatcher

		ENTER_SRW_EXCLUSIVE(config->srwlock)

		if (CModuleConfiguration::invalid)
		{
			CheckError(CModuleConfiguration::ApplyYamlConfigOverrides(context, config));
			CheckError(CModuleConfiguration::TokenizePromoteServerVars(config));
			CheckError(CModuleConfiguration::ApplyDefaults(config));
			CModuleConfiguration::invalid = FALSE;
		}

		LEAVE_SRW_EXCLUSIVE(config->srwlock)
	}

	return S_OK;
Error:
	return hr;
}

HRESULT CModuleConfiguration::GetConfig(IHttpContext* context, CModuleConfiguration** config)
{
	HRESULT hr;
	CModuleConfiguration* c = NULL;
	IAppHostElement* section = NULL;
	size_t i;
	CheckNull(config);

	*config = (CModuleConfiguration*)context->GetMetadata()->GetModuleContextContainer()->GetModuleContext(moduleId);

	CheckError(CModuleConfiguration::EnsureCurrent(context, *config));

	if (NULL == *config)
	{
		ErrorIf(NULL == (c = new CModuleConfiguration()), ERROR_NOT_ENOUGH_MEMORY);
		
		CheckError(GetConfigSection(context, &section));		
		CheckError(GetDWORD(section, L"asyncCompletionThreadCount", &c->asyncCompletionThreadCount));
		CheckError(GetDWORD(section, L"nodeProcessCountPerApplication", &c->nodeProcessCountPerApplication));
		CheckError(GetDWORD(section, L"maxConcurrentRequestsPerProcess", &c->maxConcurrentRequestsPerProcess));
		CheckError(GetDWORD(section, L"maxNamedPipeConnectionRetry", &c->maxNamedPipeConnectionRetry));
		CheckError(GetDWORD(section, L"namedPipeConnectionRetryDelay", &c->namedPipeConnectionRetryDelay));
		CheckError(GetDWORD(section, L"maxNamedPipeConnectionPoolSize", &c->maxNamedPipeConnectionPoolSize));
		CheckError(GetDWORD(section, L"maxNamedPipePooledConnectionAge", &c->maxNamedPipePooledConnectionAge));
		CheckError(GetDWORD(section, L"initialRequestBufferSize", &c->initialRequestBufferSize));
		CheckError(GetDWORD(section, L"maxRequestBufferSize", &c->maxRequestBufferSize));
		CheckError(GetDWORD(section, L"uncFileChangesPollingInterval", &c->uncFileChangesPollingInterval));
		CheckError(GetDWORD(section, L"gracefulShutdownTimeout", &c->gracefulShutdownTimeout));
		CheckError(GetDWORD(section, L"maxTotalLogFileSizeInKB", &c->maxTotalLogFileSizeInKB));
		CheckError(GetDWORD(section, L"maxLogFileSizeInKB", &c->maxLogFileSizeInKB));
		CheckError(GetDWORD(section, L"maxLogFiles", &c->maxLogFiles));
		CheckError(GetBOOL(section, L"loggingEnabled", &c->loggingEnabled, TRUE));
		CheckError(GetBOOL(section, L"devErrorsEnabled", &c->devErrorsEnabled, TRUE));
		CheckError(GetBOOL(section, L"flushResponse", &c->flushResponse, FALSE));
		CheckError(GetString(section, L"logDirectory", &c->logDirectory));
		CheckError(GetBOOL(section, L"debuggingEnabled", &c->debuggingEnabled, TRUE));
		CheckError(GetBOOL(section, L"debugHeaderEnabled", &c->debugHeaderEnabled, FALSE));
		CheckError(GetString(section, L"node_env", &c->node_env));
		CheckError(GetString(section, L"debuggerPortRange", &c->debugPortRange));
		CheckError(GetString(section, L"watchedFiles", &c->watchedFiles));
		CheckError(GetBOOL(section, L"enableXFF", &c->enableXFF, FALSE));
		CheckError(GetString(section, L"promoteServerVars", &c->promoteServerVarsRaw));
		CheckError(GetString(section, L"configOverrides", &c->configOverrides));
		CheckError(GetString(section, L"nodeProcessCommandLine", &c->nodeProcessCommandLine));
		CheckError(GetString(section, L"nodeProcessWorkingDirectory", &c->nodeProcessWorkingDirectory));
		CheckError(GetString(section, L"interceptor", &c->interceptor));

		// debuggerPathSegment

		CheckError(GetString(section, L"debuggerPathSegment", &c->debuggerPathSegment));
		c->debuggerPathSegmentLength = wcslen(c->debuggerPathSegment);

		// apply config setting overrides from the optional YAML configuration file

		CheckError(CModuleConfiguration::ApplyYamlConfigOverrides(context, c));

		// tokenize promoteServerVars

		CheckError(CModuleConfiguration::TokenizePromoteServerVars(c));

		// done with section
		
		section->Release();
		section = NULL;

        // apply defaults

		CheckError(CModuleConfiguration::ApplyDefaults(c));
		
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

LPWSTR CModuleConfiguration::GetNodeProcessCommandLine(IHttpContext* ctx)
{
	GETCONFIG(nodeProcessCommandLine)
}

LPWSTR CModuleConfiguration::GetNodeProcessWorkingDirectory(IHttpContext* ctx)
{
	GETCONFIG(nodeProcessWorkingDirectory)
}

LPWSTR CModuleConfiguration::GetInterceptor(IHttpContext* ctx)
{
	GETCONFIG(interceptor)
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

LPWSTR CModuleConfiguration::GetLogDirectory(IHttpContext* ctx)
{
	GETCONFIG(logDirectory)
}

LPWSTR CModuleConfiguration::GetDebuggerPathSegment(IHttpContext* ctx)
{
	GETCONFIG(debuggerPathSegment)
}

DWORD CModuleConfiguration::GetDebuggerPathSegmentLength(IHttpContext* ctx)
{
	GETCONFIG(debuggerPathSegmentLength)
}

DWORD CModuleConfiguration::GetMaxTotalLogFileSizeInKB(IHttpContext* ctx)
{
	GETCONFIG(maxTotalLogFileSizeInKB)
}

DWORD CModuleConfiguration::GetMaxLogFileSizeInKB(IHttpContext* ctx)
{
	GETCONFIG(maxLogFileSizeInKB)
}

DWORD CModuleConfiguration::GetMaxLogFiles(IHttpContext* ctx)
{
	GETCONFIG(maxLogFiles)
}

BOOL CModuleConfiguration::GetLoggingEnabled(IHttpContext* ctx)
{
	GETCONFIG(loggingEnabled);
}

BOOL CModuleConfiguration::GetDebuggingEnabled(IHttpContext* ctx)
{
	GETCONFIG(debuggingEnabled)
}

BOOL CModuleConfiguration::GetDebugHeaderEnabled(IHttpContext* ctx)
{
	GETCONFIG(debugHeaderEnabled)
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

LPWSTR CModuleConfiguration::GetWatchedFiles(IHttpContext* ctx)
{
	GETCONFIG(watchedFiles)
}

DWORD CModuleConfiguration::GetMaxNamedPipeConnectionPoolSize(IHttpContext* ctx)
{
	GETCONFIG(maxNamedPipeConnectionPoolSize)
}

DWORD CModuleConfiguration::GetMaxNamedPipePooledConnectionAge(IHttpContext* ctx)
{
	GETCONFIG(maxNamedPipePooledConnectionAge)
}

BOOL CModuleConfiguration::GetEnableXFF(IHttpContext* ctx)
{
	GETCONFIG(enableXFF)
}

LPWSTR CModuleConfiguration::GetConfigOverrides(IHttpContext* ctx)
{
	GETCONFIG(configOverrides)
}

HRESULT CModuleConfiguration::GetDebugPortRange(IHttpContext* ctx, DWORD* start, DWORD* end)
{
	HRESULT hr;

	CheckNull(start);
	CheckNull(end);

	CModuleConfiguration* c = NULL; 
	CheckError(GetConfig(ctx, &c));

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

HRESULT CModuleConfiguration::GetPromoteServerVars(IHttpContext* ctx, char*** vars, int* count)
{
	HRESULT hr;

	CheckNull(vars);
	CheckNull(count);

	CModuleConfiguration* c = NULL; 
	CheckError(GetConfig(ctx, &c));

	*vars = c->promoteServerVars;
	*count = c->promoteServerVarsCount;

	return S_OK;
Error:
	return hr;
}
