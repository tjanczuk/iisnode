@echo off
for /F "tokens=*" %%i in (%~dp0\..\..\version.txt) do set IISNODE_VERSION=%%i
echo #define IISNODE_VERSION ^"%IISNODE_VERSION%^"