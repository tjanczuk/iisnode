@echo off
for /F "tokens=*" %%i in (%~dp0\..\..\version.txt) do set NODEIIS_VERSION=%%i
echo ^<Include xmlns=^"http://schemas.microsoft.com/wix/2006/wi^"^>^<^?define version=^"%NODEIIS_VERSION%.0^"^?^>^</Include^>