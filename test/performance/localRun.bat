@echo off

set mode=%1
set scenario=%2

if "%mode%" neq "node" if "%mode%" neq "iisnode" goto help
if not exist "%~dp0www\%scenario%\wcat.ubr" goto help

if %mode% equ node (
    start %~dp0server.bat %mode% %scenario%
) else (
    call %~dp0server.bat %mode% %scenario%
)
echo %ERRORLEVEL%
if %ERRORLEVEL% neq 0 (
    echo FAILED. Error starting the server
    exit /b -1
)

call %~dp0client.bat localhost %scenario%
if %ERRORLEVEL% neq 0 (
    echo FAILED. Error starting the client
    exit /b -1
)

exit /b 0

:help

echo Usage: localRun.bat ^{node ^| iisnode^} ^<scenario^>
echo    node       - starts ^<scenario^> on localhost server using vanilla node.exe
echo    iisnode    - starts ^<scenario^> on localhost server using IIS 7.x and iisnode
echo    ^<scenario^> - name of one of the subdirectories of %~dp0www

exit /b -1