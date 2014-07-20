#ifndef __CFILEWATCHER_H__
#define __CFILEWATCHER_H__

class CNodeApplicationManager;
class CNodeApplication;

typedef void (*FileModifiedCallback)(CNodeApplicationManager* manager, CNodeApplication* application);

class CFileWatcher
{
private:

    typedef struct _WatchedFile {
        WCHAR* fileName;
        FileModifiedCallback callback;
        CNodeApplicationManager* manager;
        CNodeApplication* application;
        BOOL unc;
        BOOL wildcard;
        FILETIME lastWrite;
        BOOL yamlConfig;
        struct _WatchedFile* next;
    } WatchedFile;

    typedef struct _WatchedDirectory {
        WatchedFile* files;
        WCHAR* directoryName;
        HANDLE watchHandle;
        FILE_NOTIFY_INFORMATION info;
        OVERLAPPED overlapped;
        struct _WatchedDirectory* next;
    } WatchedDirectory;

    HANDLE worker;
    HANDLE completionPort;
    WatchedDirectory* directories;
    DWORD uncFileSharePollingInterval;
    CRITICAL_SECTION syncRoot;
    LPWSTR configOverridesFileName;
    DWORD configOverridesFileNameLength;

    static unsigned int WINAPI Worker(void* arg);
    BOOL ScanDirectory(WatchedDirectory* directory, BOOL unc);
    HRESULT WatchFile(PCWSTR directoryName, DWORD directoryNameLength, BOOL unc, PCWSTR startSubdirectoryName, PCWSTR startFileName, PCWSTR endFileName, BOOL wildcard);
    BOOL DirectoryExists(LPCWSTR directoryPath);
    BOOL IsWebConfigFile(PCWSTR pszStart, PCWSTR pszEnd);
    static HRESULT GetWatchedFileTimestamp(WatchedFile* file, FILETIME* timestamp);

public:

    CFileWatcher();
    ~CFileWatcher();

    HRESULT Initialize(IHttpContext* context);
    HRESULT WatchFiles(PCWSTR mainFileName, PCWSTR watchedFiles, FileModifiedCallback callback, CNodeApplicationManager* manager, CNodeApplication* application);
    HRESULT RemoveWatch(CNodeApplication* application);
};

#endif