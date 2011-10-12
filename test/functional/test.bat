@echo off
setlocal

set this=%~dp0

if "%1" equ "/?" goto :help
if "%1" equ "/help" goto :help
if "%1" equ "-h" goto :help
if "%1" equ "--help" goto :help
if "%1" equ "--h" goto :help

set log="%this%log.out"

date /T 
time /T 
date /T > %log%
time /T >> %log%

set testFilter=*
set nr=0
set ns=

:nextParam
if "%1" equ "/nr" set nr=1& shift & goto :nextParam
if "%1" equ "/ns" set ns=/ns& shift & goto :nextParam
if "%1" neq "" set testFilter=%1& shift & goto :nextParam

call %this%scripts\setup.bat %ns%
if %ERRORLEVEL% neq 0 exit /b -1

dir /b %this%tests\%testFilter% > nul 2>&1
if %ERRORLEVEL% neq 0 (
	echo No tests names match the filter %testFilter%
	exit /b -1
)

set success=0
set failure=0

for /f %%I in ('dir /b /a-d %this%tests\%testFilter%') do call :run_one %%I %%~xI

echo Total passed: %success%
echo Total failed: %failure%
echo ------------------------ >> %log%
echo Total passed: %success% >> %log%
echo Total failed: %failure% >> %log%

date /T 
time /T 
date /T >> %log%
time /T >> %log%

if %failure% neq 0 (
	echo Check %log% for details. 
	exit /b -1
)

exit /b 0


:: run one test
:: %1 - the fully qualified file name of the test file
:: %2 - the extension of the file name

:run_one

echo Running: %1...
echo ------------------------ Running %1 >> %log%

if "%nr%" equ "0" call :resetAppPool
if %ERRORLEVEL% neq 0 (
	set /A failure=failure+1
	echo Failed: %1
	echo Failed: %1 >> %log%
	exit /b -1	
)

if "%2" equ ".js" (
	call %this%scripts\runNodeTest.bat %1 >> %log% 2>>&1
) else if "%2" equ ".bat" (
	call "%this%tests\%1" >> %log% 2>>&1
)

if %ERRORLEVEL% equ 0 (
	set /A success=success+1
	echo Passed:  %1
	echo Passed:  %1 >> %log%
) else (
	set /A failure=failure+1
	echo Failed:  %1
	echo Failed:  %1 >> %log%
)

if "%nr%" equ "0" call :resetAppPool

exit /b 0

:: stops and starts the iisnodetest application pool

:resetAppPool

%appcmd% stop apppool %apppool% >> %log% 2>>&1
if %ERRORLEVEL% neq 0 if %ERRORLEVEL% neq 1062 exit /b -1
%appcmd% start apppool %apppool% >> %log% 2>>&1
if %ERRORLEVEL% neq 0 exit /b -1
exit /b 0

:: display help and exit

:help

echo test.bat [options] [testFilter]
echo options:
echo     /nr         - do not reset the application pool before every test case (useful for debugging)
echo     /ns         - do not recreate the application pool before every test run (useful for debugging)
echo testFilter      - a filename-based filter for tests to run; default *
echo e.g. test.bat 1* - run all tests from the 100 category
exit /b -1

endlocal