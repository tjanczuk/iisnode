@echo off
setlocal

set appcmd="%ProgramFiles%\IIS Express\appcmd.exe"
set schemadir="%ProgramFiles%\IIS Express\config\schema"
set node="%programfiles%\nodejs\node.exe"
if "%PROCESSOR_ARCHITECTURE%" equ "AMD64" (
	set appcmd="%ProgramFiles(x86)%\IIS Express\appcmd.exe"
	set schemadir="%ProgramFiles(x86)%\IIS Express\config\schema"
	set node="%programfiles(x86)%\nodejs\node.exe"
)

set applicationhostconfig=%userprofile%\documents\IISexpress\config\applicationhost.config
set iisnode=%~dp0iisnode.dll
set www=%~dp0www
set index=%www%\index.htm
set schema=%~dp0\iisnode_schema.xml
set addsection=%~dp0\addiisnodesectiongroup.js
set ensureiisexpressconfig=%~dp0\ensureiisexpressconfig.js
set processor_architecture_flag=%~dp0x86.txt
set wscript=%systemdrive%\windows\system32\wscript.exe

echo IIS Express module installer for iisnode - hosting of node.js applications in IIS Express.
echo This script must be run individually for every user on the machine.

if not exist "%processor_architecture_flag%" (
	echo Installation failed. IIS Express requires 32 bit build of iisnode. You can get it from https://github.com/tjanczuk/iisnode/archives/master or build yourself.
	exit /b -1
)

if not exist %appcmd% (
	echo Installation failed. The appcmd.exe IIS Express management tool was not found at %appcmd%. Make sure you have IIS Express installed.
	exit /b -1
)

if not exist "%iisnode%" (
	echo Installation failed. The iisnode.dll native module to install was not found at %iisnode%. Make sure you are running this script from the pre-built installation package rather than from the source tree.
	exit /b -1
)

if not exist "%index%" (
	echo Installation failed. The samples were not found at %www%. Make sure you are running this script from the pre-built installation package rather than from the source tree.
	exit /b -1
)

if not exist "%schema%" (
	echo Installation failed. The configuration schema was not found at %schema%. Make sure you are running this script from the pre-built installation package rather than from the source tree.
	exit /b -1
)

if not exist "%addsection%" (
	echo Installation failed. The %addsection% script required to install iisnode configuration not found. Make sure you are running this script from the pre-built installation package rather than from the source tree.
	exit /b -1
)

if not exist "%ensureiisexpressconfig%" (
	echo Installation failed. The %ensureiisexpressconfig% script required to install iisnode configuration not found. Make sure you are running this script from the pre-built installation package rather than from the source tree.
	exit /b -1
)

if not exist %node% (
	echo *****************************************************************************
	echo **************************       ERROR      *********************************
	echo   The node.exe is not found at %node%.
	echo   IIS cannot serve node.js applications without node.exe.
	echo   Please get the latest node.exe build from http://nodejs.org and 
	echo   install it to %node%, then restart the installer 
	echo *****************************************************************************
	echo *****************************************************************************
	exit /b -1
)

if "%1" neq "/s" (
	echo This installer will perform the following tasks:
	echo * unregister existing "iisnode" global module from your installation of IIS Express if such registration exists
	echo * register %iisnode% as a native module with your installation of IIS Express
	echo * install configuration schema for the "iisnode" module
	echo * remove existing "iisnode" section from system.webServer section group in %applicationhostconfig%
	echo * add the "iisnode" section within the system.webServer section group in %applicationhostconfig%
	echo This script does not provide means to revert these actions. If something fails in the middle you are on your own.
	echo Press ENTER to continue or Ctrl-C to terminate.
	pause 
)

echo Ensuring the %applicationhostconfig% IIS Express configuration is created...
"%wscript%" /B /E:jscript "%ensureiisexpressconfig%"
if %ERRORLEVEL% neq 0 (
	echo Installation failed. Cannot create %applicationhostconfig%
	exit /b -1
)
echo ...success

echo Ensuring any existing registration of 'iisnode' native module is removed...
%appcmd% uninstall module iisnode /commit:apphost
if %ERRORLEVEL% neq 0 if %ERRORLEVEL% neq 1168 (
	echo Installation failed. Cannot remove potentially existing registration of 'iisnode' IIS native module
	exit /b -1
)
echo ...success

echo Registering the iisnode native module %iisnode%...
%appcmd% install module /name:iisnode /image:"%iisnode%" /commit:apphost
if %ERRORLEVEL% neq 0 (
	echo Installation failed. Cannot register iisnode native module 
	exit /b -1
)
echo ...success

echo Installing the iisnode module configuration schema from %schema%...
copy /y "%schema%" %schemadir%
if %ERRORLEVEL% neq 0 (
	echo Installation failed. Cannot install iisnode module configuration schema
	exit /b -1
)
echo ...success

echo Registering the iisnode section within the system.webServer section group...
"%wscript%" /B /E:jscript "%addsection%" /express
if %ERRORLEVEL% neq 0 (
	echo Installation failed. Cannot register iisnode configuration section
	exit /b -1
)
echo ...success

echo INSTALLATION SUCCESSFUL. Open WebMatrix and create a new WebSite pointing to %www% to explore the samples.

endlocal
