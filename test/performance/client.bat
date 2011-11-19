@echo off

set server=%1
set scenario="%~dp0www\%2\wcat.ubr"

set wcat="%programfiles%\WCAT\wcat.wsf"

if not exist %scenario% goto help

if not exist %wcat% (
	echo FAILED. The wcat.wsf was not found at %wcat%. Install a copy of WCAT first. Check the readme.txt. 
	exit /b -1
)

cscript %wcat% -terminate -update -clients localhost
if %ERRORLEVEL% neq 0 (
    echo FAILED. Error initializing WCAT client.
	exit /b -1
)

cscript %wcat% -terminate -run -clients localhost -t %scenario% -s %server% -p 31416 -v 8 -singleip -x
if %ERRORLEVEL% neq 0 (
    echo FAILED. Error running WCAT client.
	exit /b -1
)

exit /b 0

:help

echo Usage: client.bat ^<server^> ^<scenario^>
echo    ^<server^>   - server host name
echo    ^<scenario^> - name of one of the subdirectories of %~dp0www

exit /b -1