@echo off

set server=%1
set scenario="%~dp0www\%2\wcat.ubr"
set clients=localhost
set vclients=2000

set wcat="%programfiles%\WCAT\wcat.wsf"

if [%3] neq [] set clients=%3
if "%4" neq "" set vclients=%4

if not exist %scenario% goto help

if not exist %wcat% (
	echo FAILED. The wcat.wsf was not found at %wcat%. Install a copy of WCAT first. Check the readme.txt. 
	exit /b -1
)

copy /y "%programfiles%\WCAT\report.xsl" "%~dp0"

cscript %wcat% -terminate -update -clients %clients%
if %ERRORLEVEL% neq 0 (
    echo FAILED. Error initializing WCAT client.
	exit /b -1
)

cscript %wcat% -terminate -run -clients %clients% -t %scenario% -s %server% -p 31416 -v %vclients% -singleip -x
if %ERRORLEVEL% neq 0 (
    echo FAILED. Error running WCAT client.
	exit /b -1
)

exit /b 0

:help

echo Usage: client.bat ^<server^> ^<scenario^> ^[^"^<clients^"^> ^[^<vclients^>^]^]
echo    ^<server^>   - server host name
echo    ^<scenario^> - name of one of the subdirectories of %~dp0www
echo    ^<clients^>  - comma delimited list of WCAT client machines to use; defaults to localhost
echo    ^<vclients^> - number of virtual clients per physical client; defaults to 2000

exit /b -1