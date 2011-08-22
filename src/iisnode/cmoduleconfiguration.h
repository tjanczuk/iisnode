#ifndef __CMODULECONFIGURATION_H__
#define __CMODULECONFIGURATION__H__

class CModuleConfiguration
{
private:

	static IHttpServer* server;

	CModuleConfiguration();
	~CModuleConfiguration();

public:

	static HRESULT Initialize(IHttpServer* server);

	static DWORD GetMaxPendingRequestsPerApplication();
	static DWORD GetAsyncCompletionThreadCount();
	static DWORD GetMaxProcessCountPerApplication();
	static LPCTSTR GetNodeProcessCommandLine();
	static DWORD GetMaxConcurrentRequestsPerProcess();
	static DWORD GetMaxNamedPipeConnectionRetry();
	static DWORD GetNamePipeConnectionRetryDelay(); // this is in 100 nanosecond units
	static DWORD GetInitialRequestBufferSize();
	static DWORD GetMaximumRequestBufferSize();
	static DWORD GetUNCFileChangesPollingInterval();
	static DWORD GetGracefulShutdownTimeout();
	static LPWSTR GetLogDirectoryNameSuffix();
	static DWORD GetLogFileFlushInterval();
	static DWORD GetMaxLogFileSizeInKB();
	static BOOL GetLoggingEnabled();
	static BOOL GetAppendToExistingLog();
};

#endif