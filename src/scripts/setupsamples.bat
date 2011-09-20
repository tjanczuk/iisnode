@echo off
setlocal

set appcmd=%systemroot%\system32\inetsrv\appcmd.exe
set www=%~dp0www
set index=%www%\index.htm
set siteName=Default Web Site
set path=node
set site="%siteName%/%path%"
set icacls=%systemdrive%\windows\system32\icacls.exe

echo Installation of node.js samples for IIS 7
echo This script must be run with administrative privileges.

if not exist "%icacls%" (
	echo Installation failed. The icacls.exe not found at %icacls%. 
	exit /b -1
)

if not exist %appcmd% (
	echo Installation failed. The appcmd.exe IIS management tool was not found at %appcmd%. Make sure you have both IIS7 as well as IIS7 Management Tools installed.
	exit /b -1
)

if not exist "%index%" (
	echo Installation failed. The samples were not found at "%www%". Make sure you are running this script from the pre-built installation package rather than from the source tree.
	exit /b -1
)

if "%1" neq "/s" (
	echo This installer will perform the following tasks:
	echo * ensure that the IIS_IUSRS group has read and write rights to "%www%"
	echo * delete the %site% web application if it exists
	echo * add a new site %site% to IIS with physical path pointing to "%www%"
	echo This script does not provide means to revert these actions. If something fails in the middle you are on your own.
	echo Press ENTER to continue or Ctrl-C to terminate.
	pause 
)

echo Ensuring IIS_IUSRS group has full permissions for "%www%"...
%icacls% "%www%" /grant IIS_IUSRS:(OI)(CI)F
if %ERRORLEVEL% neq 0 (
	echo Installation failed. Cannot set permissions for "%www%". 
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
	echo Installation failed. Cannot create samples site %site% at physical path "%www%"
	exit /b -1
)
echo ...success

echo INSTALLATION SUCCESSFUL. Check out the samples at http://localhost/node.

if "%1" neq "/s" pause

endlocal

