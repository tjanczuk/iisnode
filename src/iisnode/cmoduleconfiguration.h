#ifndef __CMODULECONFIGURATION_H__
#define __CMODULECONFIGURATION__H__

// Modifying the list of configuration parameters must be synchronized with changes in iisnode_schema.xml

class CModuleConfiguration : public IHttpStoredContext
{
private:

	DWORD asyncCompletionThreadCount;
	DWORD nodeProcessCountPerApplication;
	LPWSTR nodeProcessCommandLine;
	LPWSTR nodeProcessWorkingDirectory;
	LPWSTR interceptor;
	DWORD maxConcurrentRequestsPerProcess;
	DWORD maxNamedPipeConnectionRetry;
	DWORD namedPipeConnectionRetryDelay;
	DWORD initialRequestBufferSize;
	DWORD maxRequestBufferSize;
	DWORD uncFileChangesPollingInterval;
	DWORD gracefulShutdownTimeout;
	LPWSTR logDirectory;
	LPWSTR debuggerPathSegment;
	DWORD debuggerPathSegmentLength;
	DWORD maxLogFileSizeInKB;
	DWORD maxTotalLogFileSizeInKB;
	DWORD maxLogFiles;
	BOOL loggingEnabled;
	BOOL debuggingEnabled;
	BOOL debugHeaderEnabled;
	LPWSTR debugPortRange;
	DWORD debugPortStart;
	DWORD debugPortEnd;
	LPWSTR node_env;
	BOOL devErrorsEnabled;
	BOOL flushResponse;
	LPWSTR watchedFiles;
	DWORD maxNamedPipeConnectionPoolSize;
	DWORD maxNamedPipePooledConnectionAge;
	BOOL enableXFF;
	char** promoteServerVars;
	int promoteServerVarsCount;
	LPWSTR promoteServerVarsRaw;
	LPWSTR configOverridesFileName;
	static BOOL invalid;
	SRWLOCK srwlock;
	LPWSTR configOverrides;

	static IHttpServer* server;
	static HTTP_MODULE_ID moduleId;
	static HRESULT GetConfigSection(IHttpContext* context, IAppHostElement** section, OLECHAR* configElement = L"system.webServer/iisnode");
	static HRESULT GetString(IAppHostElement* section, LPCWSTR propertyName, LPWSTR* value);
	static HRESULT GetBOOL(IAppHostElement* section, LPCWSTR propertyName, BOOL* value, BOOL defaultValue);
	static HRESULT GetDWORD(char* str, DWORD* value);
	static HRESULT GetBOOL(char* str, BOOL* value);
	static HRESULT GetString(char* str, LPWSTR* value);	
	static HRESULT GetDWORD(IAppHostElement* section, LPCWSTR propertyName, DWORD* value);	
	static HRESULT ApplyConfigOverrideKeyValue(IHttpContext* context, CModuleConfiguration* config, char* keyStart, char* keyEnd, char* valueStart, char* valueEnd);
	static HRESULT ApplyYamlConfigOverrides(IHttpContext* context, CModuleConfiguration* config);
	static HRESULT TokenizePromoteServerVars(CModuleConfiguration* c);
	static HRESULT ApplyDefaults(CModuleConfiguration* c);
	static HRESULT EnsureCurrent(IHttpContext* context, CModuleConfiguration* config);

	CModuleConfiguration();
	~CModuleConfiguration();

public:

	static HRESULT Initialize(IHttpServer* server, HTTP_MODULE_ID moduleId);

	static HRESULT GetConfig(IHttpContext* context, CModuleConfiguration** config);

	static DWORD GetAsyncCompletionThreadCount(IHttpContext* ctx); 
	static DWORD GetNodeProcessCountPerApplication(IHttpContext* ctx);
	static LPWSTR GetNodeProcessCommandLine(IHttpContext* ctx); 
	static LPWSTR GetNodeProcessWorkingDirectory(IHttpContext* ctx);
	static LPWSTR GetInterceptor(IHttpContext* ctx); 
	static DWORD GetMaxConcurrentRequestsPerProcess(IHttpContext* ctx);
	static DWORD GetMaxNamedPipeConnectionRetry(IHttpContext* ctx);
	static DWORD GetNamedPipeConnectionRetryDelay(IHttpContext* ctx); 
	static DWORD GetInitialRequestBufferSize(IHttpContext* ctx);
	static DWORD GetMaxRequestBufferSize(IHttpContext* ctx);
	static DWORD GetUNCFileChangesPollingInterval(IHttpContext* ctx); 
	static DWORD GetGracefulShutdownTimeout(IHttpContext* ctx);
	static LPWSTR GetLogDirectory(IHttpContext* ctx);
	static LPWSTR GetDebuggerPathSegment(IHttpContext* ctx);
	static DWORD GetDebuggerPathSegmentLength(IHttpContext* ctx);
	static DWORD GetMaxLogFileSizeInKB(IHttpContext* ctx);
	static DWORD GetMaxTotalLogFileSizeInKB(IHttpContext* ctx);
	static DWORD GetMaxLogFiles(IHttpContext* ctx);
	static BOOL GetLoggingEnabled(IHttpContext* ctx);
	static BOOL GetDebuggingEnabled(IHttpContext* ctx);
	static BOOL GetDebugHeaderEnabled(IHttpContext* ctx);
	static HRESULT GetDebugPortRange(IHttpContext* ctx, DWORD* start, DWORD* end);
	static LPWSTR GetNodeEnv(IHttpContext* ctx);
	static BOOL GetDevErrorsEnabled(IHttpContext* ctx);
	static BOOL GetFlushResponse(IHttpContext* ctx);
	static LPWSTR GetWatchedFiles(IHttpContext* ctx);
	static DWORD GetMaxNamedPipeConnectionPoolSize(IHttpContext* ctx);
	static DWORD GetMaxNamedPipePooledConnectionAge(IHttpContext* ctx);
	static BOOL GetEnableXFF(IHttpContext* ctx);
	static HRESULT GetPromoteServerVars(IHttpContext* ctx, char*** vars, int* count);
	static LPWSTR GetConfigOverrides(IHttpContext* ctx);

	static HRESULT CreateNodeEnvironment(IHttpContext* ctx, DWORD debugPort, PCH namedPipe, PCH* env);

	static void Invalidate();

	virtual void CleanupStoredContext();
};

#endif