@echo off

set mode=%1
set scenario=%2

if "%mode%" neq "node" if "%mode%" neq "iisnode" goto help
if not exist %~dp0www\%scenario%\web.config goto help

set appcmd=%systemroot%\system32\inetsrv\appcmd.exe
set apppool=iisnodeperf
set site=iisnodeperf
set port=31416
set node="%programfiles%\nodejs\node.exe"
if "%PROCESSOR_ARCHITECTURE%" neq "x86" set node="%programfiles(x86)%\nodejs\node.exe"
set www=%~dp0www\%scenario%

if not exist %node% (
	echo FAILED. The node.exe was not found at %node%. Download a copy from http://nodejs.org.
	exit /b -1
)

if not exist %appcmd% (
	echo FAILED. The appcmd.exe IIS management tool was not found at %appcmd%. Make sure you have both IIS7 as well as IIS7 Management Tools installed.
	exit /b -1
)

%appcmd% delete site %site% > nul
if %ERRORLEVEL% neq 0 if %ERRORLEVEL% neq 1168 (
	echo FAILED. Cannot delete site %site%.
	exit /b -1
)

%appcmd% delete apppool %apppool% > nul
if %ERRORLEVEL% neq 0 if %ERRORLEVEL% neq 1168 (
	echo FAILED. Cannot delete application pool %apppool%.
	exit /b -1
)

if %mode% neq node goto iisnode 

set node_env=production

echo Starting the node.exe server... Ctrl-C to terminate...
%node% %~dp0www\%scenario%\server.js
if %ERRORLEVEL% neq 0 (
	echo FAILED. Node.exe failed.
	exit /b -1
)

exit /b 0

:iisnode

%appcmd% add apppool /name:%apppool%
if %ERRORLEVEL% neq 0 (
	echo FAILED. Cannot create application pool %apppool%.
	exit /b -1
)

%appcmd% set apppool /apppool.name:%apppool% /managedRuntimeVersion:
if %ERRORLEVEL% neq 0 (
	echo FAILED. Cannot set managed runtime version to none for application pool %apppool%.
	exit /b -1
)

%appcmd% set apppool /apppool.name:%apppool% /queueLength:65535
if %ERRORLEVEL% neq 0 (
	echo FAILED. Cannot set queue length for application pool %apppool%.
	exit /b -1
)

%appcmd% add site /name:%site% /physicalPath:"%www%" /bindings:http/*:%port%:
if %ERRORLEVEL% neq 0 (
	echo FAILED. Cannot create site %site%.
	exit /b -1
)

%appcmd% set site %site% /[path='/'].applicationPool:%apppool%
if %ERRORLEVEL% neq 0 (
	echo FAILED. Cannot configure site %site%.
	exit /b -1
)

set RETRY=0

:retry

%appcmd% start site %site%
if %ERRORLEVEL% equ 0 goto started
set /a RETRY+=1
if %RETRY% equ 3 (
	echo FAILED. Cannot start site %site% after %RETRY% retries.
	exit /b -1
)
timeout /T 3 /NOBREAK
echo Retrying to start the site %site%...
goto retry

:started

echo IIS server started. Start the performance test clients.

exit /b 0

:help

echo Usage: server.bat ^{node ^| iisnode^} ^<scenario^>
echo    node       - starts ^<scenario^> server using vanilla node.exe
echo    iisnode    - starts ^<scenario^> server using IIS 7.x and iisnode
echo    ^<scenario^> - name of one of the subdirectories of %~dp0www

exit /b -1