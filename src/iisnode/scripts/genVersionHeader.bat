@echo off
for /F "tokens=*" %%i in (%~dp0\..\..\version.txt) do set IISNODE_VERSION=%%i
set IISNODE_VERSION_LONG=%IISNODE_VERSION:.=,%
echo #define IISNODE_VERSION ^"%IISNODE_VERSION%^"
echo #define IISNODE_VERSION_LONG %IISNODE_VERSION_LONG%,0