#include "precomp.h"

CFileWatcher::CFileWatcher()
    : completionPort(NULL), worker(NULL), directories(NULL), uncFileSharePollingInterval(0), 
    configOverridesFileName(NULL), configOverridesFileNameLength(0)
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
        if(currentDirectory->watchHandle != NULL)
        {
            CloseHandle(currentDirectory->watchHandle);
            currentDirectory->watchHandle = NULL;
        }
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

    if (NULL != this->configOverridesFileName)
    {
        delete [] this->configOverridesFileName;
        this->configOverridesFileName = NULL;
    }

    DeleteCriticalSection(&this->syncRoot);
}

HRESULT CFileWatcher::Initialize(IHttpContext* context)
{
    HRESULT hr;

    ErrorIf(NULL == (this->completionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1)), GetLastError());		
    this->uncFileSharePollingInterval = CModuleConfiguration::GetUNCFileChangesPollingInterval(context);

    LPWSTR overrides = CModuleConfiguration::GetConfigOverrides(context);
    if (overrides && *overrides != L'\0')
    {
        this->configOverridesFileNameLength = wcslen(overrides);
        ErrorIf(NULL == (this->configOverridesFileName = new WCHAR[this->configOverridesFileNameLength + 1]), ERROR_NOT_ENOUGH_MEMORY);
        wcscpy(this->configOverridesFileName, overrides);
    }

    return S_OK;
Error:

    if (NULL != this->completionPort)
    {
        CloseHandle(this->completionPort);
        this->completionPort = NULL;
    }

    return hr;
}

BOOL CFileWatcher::IsWebConfigFile(PCWSTR pszStart, PCWSTR pszEnd)
{
    DWORD dwEndIndex = pszEnd - pszStart;
    
    if(dwEndIndex < 10)
    {
        return FALSE;
    }

    return _wcsnicmp((pszStart + ( dwEndIndex - 10 )), L"web.config", 10) == 0;
}

HRESULT CFileWatcher::WatchFiles(PCWSTR mainFileName, PCWSTR watchedFiles, FileModifiedCallback callback, CNodeApplicationManager* manager, CNodeApplication* application)
{
    HRESULT hr;
    WCHAR fileOnly[_MAX_FNAME];
    WCHAR ext[_MAX_EXT];
    WCHAR* directoryName = NULL;
    DWORD fileNameLength;
    DWORD fileNameOnlyLength;
    DWORD directoryLength;	
    BOOL unc;
    BOOL wildcard;
    PWSTR startSubdirectory;
    PWSTR startFile;
    PWSTR endFile;
    WatchedDirectory* directory;
    WatchedFile* file;

    CheckNull(mainFileName);
    CheckNull(watchedFiles);

    // create and normalize a copy of directory name, determine if it is UNC share

    fileNameLength = wcslen(mainFileName);
    ErrorIf(0 != _wsplitpath_s(mainFileName, NULL, 0, NULL, 0, fileOnly, _MAX_FNAME, ext, _MAX_EXT), ERROR_INVALID_PARAMETER);	
    fileNameOnlyLength = wcslen(fileOnly) + wcslen(ext);	
    directoryLength = fileNameLength - fileNameOnlyLength; 
    ErrorIf(NULL == (directoryName = new WCHAR[directoryLength + 8]), ERROR_NOT_ENOUGH_MEMORY); // pessimistic length after normalization with prefix \\?\UNC\ 

    if (fileNameLength > 8 && 0 == memcmp(mainFileName, L"\\\\?\\UNC\\", 8 * sizeof WCHAR))
    {
        // normalized UNC path
        unc = TRUE;
        memcpy(directoryName, mainFileName, directoryLength * sizeof WCHAR);
        directoryName[directoryLength] = L'\0';
    }
    else if (fileNameLength > 4 && 0 == memcmp(mainFileName, L"\\\\?\\", 4 * sizeof WCHAR))
    {
        // normalized local file
        unc = FALSE;
        memcpy(directoryName, mainFileName, directoryLength * sizeof WCHAR);
        directoryName[directoryLength] = L'\0';
    }
    else if (fileNameLength > 2 && 0 == memcmp(mainFileName, L"\\\\", 2 * sizeof(WCHAR)))
    {
        // not normalized UNC path
        unc = TRUE;
        wcscpy(directoryName, L"\\\\?\\UNC\\");
        memcpy(directoryName + 8, mainFileName + 2, (directoryLength - 2) * sizeof WCHAR);
        directoryName[8 + directoryLength - 2] = L'\0';
    }
    else
    {
        // not normalized local file
        unc = FALSE;
        wcscpy(directoryName, L"\\\\?\\");
        memcpy(directoryName + 4, mainFileName, directoryLength * sizeof WCHAR);
        directoryName[4 + directoryLength] = L'\0';	
    }

    directoryLength = wcslen(directoryName);

    ENTER_CS(this->syncRoot)

    // parse watchedFiles and create a file listener for each of the files

    startFile = (PWSTR)watchedFiles;
    do {
        endFile = startSubdirectory = startFile;
        wildcard = FALSE;
        while (*endFile && *endFile != L';')
        {
            wildcard |= *endFile == L'*' || *endFile == L'?';
            if (*endFile == L'\\')
            {
                startFile = endFile + 1;
            }

            endFile++;
        }

        if (startFile != endFile)
        {
            //
            // skip web.config file (GlobalFileChangeNotification will handle web.config).
            //

            if(IsWebConfigFile(startFile, endFile))
            {
                hr = S_OK;
            }
            else
            {
                hr = this->WatchFile(directoryName, directoryLength, unc, startSubdirectory, startFile, endFile, wildcard);
            }

            // ignore files and directories that do not exist instead of failing

            if (S_OK != hr && ERROR_FILE_NOT_FOUND != hr)
            {
                // still under lock remove file watch entries that were just created, then do regular cleanup

                this->RemoveWatch(NULL);
                CheckError(hr);
            }
        }

        startFile = endFile + 1;

    } while(*endFile);

    // update temporary entries with application and callback pointers

    directory = this->directories;
    while (NULL != directory)
    {
        file = directory->files;
        while (NULL != file)
        {
            if (NULL == file->application)
            {
                file->application = application;
                file->manager = manager;
                file->callback = callback;
            }

            file = file->next;
        }

        directory = directory->next;
    }

    LEAVE_CS(this->syncRoot)

    delete [] directoryName;

    return S_OK;

Error:

    if (NULL != directoryName)
    {
        delete [] directoryName;
        directoryName = NULL;
    }


    return hr;
}

HRESULT CFileWatcher::GetWatchedFileTimestamp(WatchedFile* file, FILETIME* timestamp)
{
    HRESULT hr;
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    WIN32_FIND_DATAW findData;
    HANDLE foundFile = INVALID_HANDLE_VALUE;

    if (file->wildcard)
    {
        // a timestamp of a wildcard watched file is the XOR of the timestamps of all matching files and their names and sizes;
        // that way if any of the matching files changes, or matching files are added or removed, the timestamp will change as well

        RtlZeroMemory(timestamp, sizeof FILETIME);
        foundFile = FindFirstFileW(file->fileName, &findData);
        if (INVALID_HANDLE_VALUE != foundFile)
        {
            do
            {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    timestamp->dwHighDateTime ^= findData.ftLastWriteTime.dwHighDateTime ^ findData.nFileSizeHigh;
                    timestamp->dwLowDateTime ^= findData.ftLastWriteTime.dwLowDateTime ^ findData.nFileSizeLow;
                    WCHAR* current = findData.cFileName;
                    while (*current)
                    {
                        timestamp->dwLowDateTime ^= *current;
                        current++;
                    }
                }
            } while (FindNextFileW(foundFile, &findData));

            ErrorIf(ERROR_NO_MORE_FILES != (hr = GetLastError()), hr);
            FindClose(foundFile);
            foundFile = NULL;
        }
    }
    else
    {
        ErrorIf(!GetFileAttributesExW(file->fileName, GetFileExInfoStandard, &attributes), GetLastError());
        memcpy(timestamp, &attributes.ftLastWriteTime, sizeof attributes.ftLastWriteTime);
    }

    return S_OK;
Error:

    if (INVALID_HANDLE_VALUE != foundFile)
    {
        FindClose(foundFile);
        foundFile = INVALID_HANDLE_VALUE;
    }

    return hr;
}

HRESULT CFileWatcher::WatchFile(PCWSTR directoryName, DWORD directoryNameLength, BOOL unc, PCWSTR startSubdirectoryName, PCWSTR startFileName, PCWSTR endFileName, BOOL wildcard)
{
    HRESULT hr;
    WatchedFile* file = NULL;
    WatchedDirectory* directory;
    WatchedDirectory* newDirectory = NULL;

    // allocate new WatchedFile, get snapshot of the last write time

    ErrorIf(NULL == (file = new WatchedFile), ERROR_NOT_ENOUGH_MEMORY);
    RtlZeroMemory(file, sizeof WatchedFile);
    ErrorIf(NULL == (file->fileName = new WCHAR[directoryNameLength + endFileName - startSubdirectoryName + 1]), ERROR_NOT_ENOUGH_MEMORY);
    wcscpy(file->fileName, directoryName);
    memcpy((void*)(file->fileName + directoryNameLength), startSubdirectoryName, (endFileName - startSubdirectoryName) * sizeof WCHAR);
    file->fileName[directoryNameLength + endFileName - startSubdirectoryName] = L'\0';
    file->unc = unc;
    file->wildcard = wildcard;
    this->GetWatchedFileTimestamp(file, &file->lastWrite);

    // determine if this is the yaml config file

    if (this->configOverridesFileNameLength == (endFileName - startFileName))
    {
        file->yamlConfig = 0 == memcmp(this->configOverridesFileName, startFileName, this->configOverridesFileNameLength * sizeof WCHAR);
    }

    // find matching directory watcher entry

    directory = this->directories;
    while (NULL != directory)
    {
        if (wcslen(directory->directoryName) == directoryNameLength + startFileName - startSubdirectoryName
            && 0 == wcsncmp(directory->directoryName, directoryName, directoryNameLength)
            && 0 == wcsncmp(directory->directoryName + directoryNameLength, startSubdirectoryName, startFileName - startSubdirectoryName))
        {
            break;
        }

        directory = directory->next;
    }

    // if directory watcher not found, create one

    if (NULL == directory)
    {
        ErrorIf(NULL == (newDirectory = new WatchedDirectory), ERROR_NOT_ENOUGH_MEMORY);	
        RtlZeroMemory(newDirectory, sizeof WatchedDirectory);
        ErrorIf(NULL == (newDirectory->directoryName = new WCHAR[directoryNameLength + startFileName - startSubdirectoryName + 1]), ERROR_NOT_ENOUGH_MEMORY);
        wcscpy(newDirectory->directoryName, directoryName);
        if (startFileName > startSubdirectoryName)
        {
            wcsncat(newDirectory->directoryName, startSubdirectoryName, startFileName - startSubdirectoryName);
        }

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

        if (NULL == this->worker)
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
            newDirectory->watchHandle = NULL;
        }

        delete newDirectory;
    }

    if (NULL != file)
    {
        if (NULL != file->fileName)
        {
            delete [] file->fileName;
        }

        delete file;
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

        while (file)
        {
            if (file->application == application)
            {
                WatchedFile* tmpFile = file;
                delete [] file->fileName;
                if (previousFile)
                {
                    previousFile->next = file->next;
                    file = file->next;
                    if (!file)
                    {
                        previousDirectory = directory;
                        directory = directory->next;
                    }
                }
                else
                {
                    directory->files = file->next;
                    file = file->next;
                    if (!directory->files)
                    {
                        delete [] directory->directoryName;
                        if(directory->watchHandle != NULL)
                        {
                            CloseHandle(directory->watchHandle);
                            directory->watchHandle = NULL;
                        }

                        if (previousDirectory)
                        {
                            previousDirectory->next = directory->next;
                        }
                        else
                        {
                            this->directories = directory->next;
                        }

                        WatchedDirectory* tmpDirectory = directory;
                        directory = directory->next;
                        delete tmpDirectory;
                    }
                }

                delete tmpFile;
            }
            else
            {
                previousFile = file;
                file = file->next;
                if (!file)
                {
                    previousDirectory = directory;
                    directory = directory->next;
                }
            }
        }
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

            if (current  && current->watchHandle != NULL)
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
                //
                // watched directory exists, check if handle is valid, if not, create one.
                // watchHandle will be NULL if the watched directory was deleted before.
                //
                if(watcher->DirectoryExists(current->directoryName))
                {
                    if(current->watchHandle == NULL)
                    {
                        current->watchHandle = CreateFileW(
                            current->directoryName,           
                            FILE_LIST_DIRECTORY,    
                            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            NULL, 
                            OPEN_EXISTING,         
                            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
                            NULL);
                    }

                    //
                    // scan the directory for file changes
                    //
                    if (watcher->ScanDirectory(current, TRUE))
                    {
                        //
                        // found a change that will recycle the application.
                        //
                        break;
                    }
                }
                else
                {
                    //
                    // directory being watched was deleted, close the handle.
                    //
                    if(current->watchHandle != NULL)
                    {
                        CloseHandle(current->watchHandle);
                        current->watchHandle = NULL;
                    }
                }
                current = current->next;
            }

            LEAVE_CS(watcher->syncRoot)
        }		
    }

    return 0;
}

BOOL CFileWatcher::DirectoryExists(LPCWSTR directoryPath)
{
    DWORD dwFileAttributes;

    dwFileAttributes = GetFileAttributesW(directoryPath);

    return ((dwFileAttributes != INVALID_FILE_ATTRIBUTES) && 
        (dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY));
}

BOOL CFileWatcher::ScanDirectory(WatchedDirectory* directory, BOOL unc)
{
    WatchedFile* file = directory->files;
    FILETIME timestamp;

    while (file)
    {
        if (unc == file->unc) 
        {
            if (S_OK == CFileWatcher::GetWatchedFileTimestamp(file, &timestamp)
                && 0 != memcmp(&timestamp, &file->lastWrite, sizeof FILETIME))
            {
                if (file->yamlConfig)
                {
                    // the iisnode.yml file has changed
                    // invalidate the configuration such that on next message it will be re-created

                    CModuleConfiguration::Invalidate();
                }

                memcpy(&file->lastWrite, &timestamp, sizeof FILETIME);
                file->callback(file->manager, file->application);
                return TRUE;
            }
        }
        file = file->next;
    }

    return FALSE;
}
