@echo off
setlocal

set log="%~dp0log.out"

date /T 
time /T 
date /T > %log%
time /T >> %log%

call %~dp0scripts\setup.bat
if %ERRORLEVEL% neq 0 exit /b -1

set testFilter=*
if "%1" neq "" set testFilter=%1

dir /b %~dp0tests\%testFilter%.* 2> nul > nul
if %ERRORLEVEL% neq 0 (
	echo No tests names match the filter %testFilter%.
	exit /b -1
)

set success=0
set failure=0

for /f %%I in ('dir /b /a-d %~dp0tests\%testFilter%.*') do call :run_one %%I %%~xI

echo Total passed: %success%
echo Total failed: %failure%
echo ------------------------ >> %log%
echo Total passed: %success% >> %log%
echo Total failed: %failure% >> %log%

date /T 
time /T 
date /T >> %log%
time /T >> %log%

if %failure% neq 0 exit /b -1

exit /b 0

:run_one

echo Running: %1...
echo ------------------------ Running %1 >> %log%
if "%2" equ ".js" (
	call :run_node_test %1
) else (
	call "%~dp0tests\%1" >> %log% 2>>&1
)

if %ERRORLEVEL% equ 0 (
	set /A success=success+1
	echo Passed: %1
	echo Passed: %1 >> %log%
) else (
	set /A failure=failure+1
	echo Failed: %1
	echo Failed: %1 >> %log%
)

exit /b 0

:run_node_test

%node% "%~dp0tests\%1" >> %log% 2>"%~dp0tests\%1.err"
type "%~dp0tests\%1.err" >> %log%
for /F %%A IN ('dir /s /b "%~dp0tests\%1.err"') do set size=%%~zA
del /q "%~dp0tests\%1.err"
if "%size%" neq "0" exit /b -1
exit /b 0

endlocal