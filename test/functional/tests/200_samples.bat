set IISNODETEST_PORT=80

if "%ns%" neq "/ns" (
	%appcmd% stop apppool DefaultAppPool
	%appcmd% start apppool DefaultAppPool
)

call %this%scripts\runNodeTest.bat parts\200_samples.js
set STATUS=%ERRORLEVEL%

if "%nr%" equ "0" (
	%appcmd% stop apppool DefaultAppPool
	%appcmd% start apppool DefaultAppPool
)

set IISNODETEST=

exit /b %STATUS%