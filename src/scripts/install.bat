@echo off
setlocal

set appcmd=%systemroot%\system32\inetsrv\appcmd.exe
set iisnode=%~dp0iisnode.dll
set www=%~dp0www
set index=%www%\index.htm
set schema=%~dp0\iisnode_schema.xml
set addsection=%~dp0\addiisnodesectiongroup.js
set siteName=Default Web Site
set path=node
set site="%siteName%/%path%"
set node=%systemdrive%\node\node.exes

echo iisnode module installer - hosting of node.js applications in IIS

if not exist %appcmd% (
	echo Installation failed. The appcmd.exe IIS management tool was not found at %appcmd%. Make sure you have both IIS7 as well as IIS7 Management Tools installed.
	exit /b -1
)

if not exist %iisnode% (
	echo Installation failed. The iisnode.dll native module to install was not found at %iisnode%. Make sure you are running this script from the pre-built installation package rather than from the source tree.
	exit /b -1
)

if not exist %index% (
	echo Installation failed. The samples were not found at %www%. Make sure you are running this script from the pre-built installation package rather than from the source tree.
	exit /b -1
)

if not exist %schema% (
	echo Installation failed. The configuration schema was not found at %schema%. Make sure you are running this script from the pre-built installation package rather than from the source tree.
	exit /b -1
)

if not exist %addsection% (
	echo Installation failed. The %addsection% script required to install iisnode configuration not found. Make sure you are running this script from the pre-built installation package rather than from the source tree.
	exit /b -1
)

echo Ensuring any existing registration of 'iisnode' native module is removed...
%appcmd% set config  -section:system.webServer/globalModules /-"[name='iisnode']" /commit:apphost
if %ERRORLEVEL% neq 0 if %ERRORLEVEL% neq 4312 (
	echo Installation failed. Cannot remove potentially existing registration of 'iisnode' IIS native module
	exit /b -1
)
echo ...success

echo Registering the iisnode native module %iisnode%...
%appcmd% set config  -section:system.webServer/globalModules /+"[name='iisnode',image='%iisnode%',preCondition='bitness32']" /commit:apphost
if %ERRORLEVEL% neq 0 (
	echo Installation failed. Cannot register iisnode native module 
	exit /b -1
)
echo ...success

echo Installing the iisnode module configuration schema from %schema%...
copy /y %schema% %systemroot%\system32\inetsrv\config\schema
if %ERRORLEVEL% neq 0 (
	echo Installation failed. Cannot install iisnode module configuration schema
	exit /b -1
)
echo ...success

echo Registering the iisnode section within the system.webServer section group...
C:\Windows\System32\wscript.exe /B %addsection%
if %ERRORLEVEL% neq 0 (
	echo Installation failed. Cannot register iisnode configuration section
	exit /b -1
)
echo ...success

echo Ensuring the %site% is removed if it exists...
%appcmd% delete app %site%
if %ERRORLEVEL% neq 0 if %ERRORLEVEL% neq 50 (
	echo Installation failed. Cannot ensure site %site% is removed
	exit /b -1
)
echo ...success

echo Creating IIS site %site% with node.js samples...
%appcmd% add app /site.name:"%siteName%" /path:/%path% /physicalPath:"%www%"
if %ERRORLEVEL% neq 0 (
	echo Installation failed. Cannot create samples site %site% at physical path %www%
	exit /b -1
)
echo ...success

echo Checking for %node% executable...
if not exist %node% (
	echo *****************************************************************************
	echo *************************       WARNING      ********************************
	echo   The node.exe is not found at %node%.
	echo   IIS cannot serve node.js applications without node.exe.
	echo   Please get a latest node.exe build from http://nodejs.org and 
	echo   install it to %node% [you can also adjust this location 
	echo   through iisnode module configuration - check the samples]
	echo *****************************************************************************
	echo *****************************************************************************
) else (
	echo ...success
)

echo INSTALLATION SUCCESSFUL. Check the samples at http://localhost/node.

endlocal
