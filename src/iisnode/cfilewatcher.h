#ifndef __CFILEWATCHER_H__
#define __CFILEWATCHER_H__

typedef void (*FileModifiedCallback)(PCWSTR fileName, void* data);

class CFileWatcher
{
private:

	typedef struct _WatchedFile {
		WCHAR* fileName;
		FileModifiedCallback callback;
		void* data;
		BOOL unc;
		FILETIME lastWrite;
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

	static unsigned int WINAPI Worker(void* arg);
	void ScanDirectory(WatchedDirectory* directory, BOOL unc);

public:

	CFileWatcher();
	~CFileWatcher();

	HRESULT Initialize();
	HRESULT WatchFile(PCWSTR fileName, FileModifiedCallback callback, void* data);
};

#endif