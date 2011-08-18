#include "precomp.h"

CFileWatcher::CFileWatcher()
	: completionPort(NULL), worker(NULL), directories(NULL)
{
}

CFileWatcher::~CFileWatcher()
{
	if (NULL != this->worker)
	{
		PostQueuedCompletionStatus(this->completionPort, 0, -1L, NULL);
		WaitForSingleObject(this->worker, INFINITE);
		CloseHandle(this->worker);
		this->worker = NULL;
	}

	if (NULL != this->completionPort)
	{
		CloseHandle(this->completionPort);
		this->completionPort = NULL;
	}

	while (NULL != this->directories)
	{
		WatchedDirectory* currentDirectory = this->directories;
		CloseHandle(currentDirectory->watchHandle);
		delete currentDirectory->directoryName;
		while (NULL != currentDirectory->files)
		{
			WatchedFile* currentFile = currentDirectory->files;
			delete currentFile->fileName;
			currentDirectory->files = currentFile->next;
			delete currentFile;
		}
		this->directories = currentDirectory->next;
		delete currentDirectory;
	}
}

HRESULT CFileWatcher::Initialize()
{
	HRESULT hr;

	ErrorIf(NULL == (this->completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1)), GetLastError());		

	return S_OK;
Error:

	if (NULL != this->completionPort)
	{
		CloseHandle(this->completionPort);
		this->completionPort = NULL;
	}

	return hr;
}

HRESULT CFileWatcher::WatchFile(PCWSTR fileName, FileModifiedCallback callback, void* data)
{
	HRESULT hr;
	WatchedFile* file;
	WatchedDirectory* directory;
	WatchedDirectory* newDirectory;
	WCHAR fileOnly[_MAX_FNAME];
	WCHAR ext[_MAX_EXT];
	WCHAR* directoryName = NULL;
	DWORD fileNameLength;
	DWORD fileNameOnlyLength;
	DWORD directoryLength;	
	WIN32_FILE_ATTRIBUTE_DATA attributes;

	CheckNull(callback);
	CheckNull(fileName);
	fileNameLength = wcslen(fileName);

	// allocate new WatchedFile, get snapshot of the last write time

	ErrorIf(NULL == (file = new WatchedFile), ERROR_NOT_ENOUGH_MEMORY);
	RtlZeroMemory(file, sizeof WatchedFile);
	file->callback = callback;
	file->data = data;
	ErrorIf(!GetFileAttributesExW(fileName, GetFileExInfoStandard, &attributes), GetLastError());
	memcpy(&file->lastWrite, &attributes.ftLastWriteTime, sizeof attributes.ftLastWriteTime);

	// create and normalize a copy of directory name and file name

	ErrorIf(0 != _wsplitpath_s(fileName, NULL, 0, NULL, 0, fileOnly, _MAX_FNAME, ext, _MAX_EXT), ERROR_INVALID_PARAMETER);	
	fileNameOnlyLength = wcslen(fileOnly) + wcslen(ext);	
	directoryLength = fileNameLength - fileNameOnlyLength; 
	ErrorIf(NULL == (directoryName = new WCHAR[directoryLength + 8]), ERROR_NOT_ENOUGH_MEMORY); // pessimistic length after normalization with prefix \\?\UNC\ 
	ErrorIf(NULL == (file->fileName = new WCHAR[fileNameLength + 1]), ERROR_NOT_ENOUGH_MEMORY);
	wcscpy(file->fileName, fileName);

	if (fileNameLength > 8 && 0 == memcmp(fileName, L"\\\\?\\UNC\\", 8 * sizeof WCHAR))
	{
		// normalized UNC path
		file->unc = TRUE;
		memcpy(directoryName, fileName, directoryLength * sizeof WCHAR);
		directoryName[directoryLength] = L'\0';
	}
	else if (fileNameLength > 4 && 0 == memcmp(fileName, L"\\\\?\\", 4 * sizeof WCHAR))
	{
		// normalized local file
		file->unc = FALSE;
		memcpy(directoryName, fileName, directoryLength * sizeof WCHAR);
		directoryName[directoryLength] = L'\0';
	}
	else if (fileNameLength > 2 && 0 == memcmp(fileName, L"\\\\", 2 * sizeof(WCHAR)))
	{
		// not normalized UNC path
		file->unc = TRUE;
		wcscpy(directoryName, L"\\\\?\\UNC\\");
		memcpy(directoryName + 8, fileName + 2, (directoryLength - 2) * sizeof WCHAR);
		directoryName[8 + directoryLength - 2] = L'\0';
	}
	else
	{
		// not normalized local file
		file->unc = FALSE;
		wcscpy(directoryName, L"\\\\?\\");
		memcpy(directoryName + 4, fileName, directoryLength * sizeof WCHAR);
		directoryName[4 + directoryLength] = L'\0';	
	}

	// find matching directory watcher entry

	directory = this->directories;
	while (NULL != directory)
	{
		if (0 == wcscmp(directory->directoryName, directoryName))
		{
			delete [] directoryName;
			directoryName = NULL;
			break;
		}
	}

	// if directory watcher not found, create one

	if (NULL == directory)
	{
		ErrorIf(NULL == (newDirectory = new WatchedDirectory), ERROR_NOT_ENOUGH_MEMORY);	
		RtlZeroMemory(newDirectory, sizeof WatchedDirectory);
		newDirectory->directoryName = directoryName;
		directoryName = NULL;
		newDirectory->files = file;

		ErrorIf(INVALID_HANDLE_VALUE == (newDirectory->watchHandle = CreateFileW(
			newDirectory->directoryName,           
			FILE_LIST_DIRECTORY,    
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL, 
			OPEN_EXISTING,         
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
			NULL)),
			GetLastError());

		ErrorIf(NULL == CreateIoCompletionPort(newDirectory->watchHandle, this->completionPort, (ULONG_PTR)newDirectory, 0), 
			GetLastError());

		ErrorIf(0 == ReadDirectoryChangesW(
			newDirectory->watchHandle,
			&newDirectory->info,
			sizeof newDirectory->info,
			FALSE,
			FILE_NOTIFY_CHANGE_LAST_WRITE,
			NULL,
			&newDirectory->overlapped,
			NULL),
			GetLastError());

		if (NULL == this->directories)
		{
			// no watchers exist yet, start the watcher thread

			ErrorIf(NULL == (this->worker = (HANDLE)_beginthreadex(
				NULL, 
				4096, 
				CFileWatcher::Worker, 
				this, 
				0, 
				NULL)), 
				ERROR_NOT_ENOUGH_MEMORY);
		}

		newDirectory->next = this->directories;
		this->directories = newDirectory;
		newDirectory = NULL;
	}
	else
	{
		file->next = directory->files;
		directory->files = file;
		file = NULL;
	}

	return S_OK;
Error:

	if (NULL != newDirectory)
	{
		if (NULL != newDirectory->directoryName)
		{
			delete [] newDirectory->directoryName;
		}

		if (NULL != newDirectory->watchHandle)
		{
			CloseHandle(newDirectory->watchHandle);
		}

		delete newDirectory;
	}

	if (NULL != file)
	{
		delete [] file->fileName;
		delete file;
	}

	if (NULL != directoryName)
	{
		delete [] directoryName;
	}

	return hr;
}

unsigned int CFileWatcher::Worker(void* arg)
{
	CFileWatcher* watcher = (CFileWatcher*)arg;
	DWORD error;
	DWORD bytes;
	ULONG_PTR key;
	LPOVERLAPPED overlapped;

	while (TRUE)
	{
		error = S_OK;
		if (!GetQueuedCompletionStatus(
			watcher->completionPort, &bytes, &key, &overlapped, CModuleConfiguration::GetUNCFileChangesPollingInterval()))
		{
			error = GetLastError();
		}

		if (-1L == key) // terminate thread
		{
			break;
		}
		else if (S_OK == error) // a change in registered directory detected (local file system)
		{
			WatchedDirectory* directory = (WatchedDirectory*)key;
			
			watcher->ScanDirectory(directory, FALSE);

			RtlZeroMemory(&directory->overlapped, sizeof directory->overlapped);
			ReadDirectoryChangesW(
				directory->watchHandle,
				&directory->info,
				sizeof directory->info,
				FALSE,
				FILE_NOTIFY_CHANGE_LAST_WRITE,
				NULL,
				&directory->overlapped,
				NULL);
		}
		else // timeout - scan all registered UNC files for changes
		{
			WatchedDirectory* current = watcher->directories;
			while (current)
			{
				watcher->ScanDirectory(current, TRUE);
				current = current->next;
			}
		}		
	}

	return 0;
}

void CFileWatcher::ScanDirectory(WatchedDirectory* directory, BOOL unc)
{
	WatchedFile* file = directory->files;
	WIN32_FILE_ATTRIBUTE_DATA attributes;

	while (file)
	{
		if (unc == file->unc) 
		{
			if (GetFileAttributesExW(file->fileName, GetFileExInfoStandard, &attributes)
				&& 0 != memcmp(&attributes.ftLastWriteTime, &file->lastWrite, sizeof FILETIME))
			{
				memcpy(&file->lastWrite, &attributes.ftLastWriteTime, sizeof FILETIME);
				file->callback(file->fileName, file->data);
			}
		}
		file = file->next;
	}
}