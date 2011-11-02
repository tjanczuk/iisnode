#include "precomp.h"

CFileWatcher::CFileWatcher()
	: completionPort(NULL), worker(NULL), directories(NULL), uncFileSharePollingInterval(0)
{
	InitializeCriticalSection(&this->syncRoot);
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
		delete [] currentDirectory->directoryName;
		while (NULL != currentDirectory->files)
		{
			WatchedFile* currentFile = currentDirectory->files;
			delete [] currentFile->fileName;
			currentDirectory->files = currentFile->next;
			delete currentFile;
		}
		this->directories = currentDirectory->next;
		delete currentDirectory;
	}

	DeleteCriticalSection(&this->syncRoot);
}

HRESULT CFileWatcher::Initialize(IHttpContext* context)
{
	HRESULT hr;

	ErrorIf(NULL == (this->completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1)), GetLastError());		
	this->uncFileSharePollingInterval = CModuleConfiguration::GetUNCFileChangesPollingInterval(context);

	return S_OK;
Error:

	if (NULL != this->completionPort)
	{
		CloseHandle(this->completionPort);
		this->completionPort = NULL;
	}

	return hr;
}

HRESULT CFileWatcher::WatchFile(PCWSTR fileName, FileModifiedCallback callback, CNodeApplicationManager* manager, CNodeApplication* application)
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
	file->manager = manager;
	file->application = application;
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

	ENTER_CS(this->syncRoot)

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

		directory = directory->next;
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

	LEAVE_CS(this->syncRoot)

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

HRESULT CFileWatcher::RemoveWatch(CNodeApplication* application)
{
	ENTER_CS(this->syncRoot)

	WatchedDirectory* directory = this->directories;
	WatchedDirectory* previousDirectory = NULL;
	while (directory)
	{
		WatchedFile* file = directory->files;
		WatchedFile* previousFile = NULL;
		while (file && file->application != application)
		{
			previousFile = file;
			file = file->next;
		}

		if (file)
		{
			delete [] file->fileName;
			if (previousFile)
			{
				previousFile->next = file->next;
			}
			else
			{
				directory->files = file->next;
			}

			delete file;

			if (!directory->files)
			{
				delete [] directory->directoryName;
				CloseHandle(directory->watchHandle);

				if (previousDirectory)
				{
					previousDirectory->next = directory->next;
				}
				else
				{
					this->directories = directory->next;
				}

				delete directory;
			}

			break;
		}

		previousDirectory = directory;
		directory = directory->next;
	}

	LEAVE_CS(this->syncRoot)

	return S_OK;
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
			watcher->completionPort, &bytes, &key, &overlapped, watcher->uncFileSharePollingInterval))
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
			
			ENTER_CS(watcher->syncRoot)

			// make sure the directory is still watched

			WatchedDirectory* current = watcher->directories;
			while (current && current != directory)
				current = current->next;

			if (current)
			{
				watcher->ScanDirectory(current, FALSE);

				// make sure the directory is still watched - it could have been removed by a recursive call to RemoveWatch

				current = watcher->directories;
				while (current && current != directory)
					current = current->next;

				if (current)
				{
					RtlZeroMemory(&current->overlapped, sizeof current->overlapped);
					ReadDirectoryChangesW(
						current->watchHandle,
						&current->info,
						sizeof directory->info,
						FALSE,
						FILE_NOTIFY_CHANGE_LAST_WRITE,
						NULL,
						&current->overlapped,
						NULL);
				}
			}

			LEAVE_CS(watcher->syncRoot)
		}
		else // timeout - scan all registered UNC files for changes
		{
			ENTER_CS(watcher->syncRoot)

			WatchedDirectory* current = watcher->directories;
			while (current)
			{
				if (watcher->ScanDirectory(current, TRUE))
					break;
				current = current->next;
			}

			LEAVE_CS(watcher->syncRoot)
		}		
	}

	return 0;
}

BOOL CFileWatcher::ScanDirectory(WatchedDirectory* directory, BOOL unc)
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
				file->callback(file->manager, file->application);
				return TRUE;
			}
		}
		file = file->next;
	}

	return FALSE;
}
