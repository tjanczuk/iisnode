@echo off
setlocal
echo ETW listener for iisnode.
set ETL=%TEMP%\iisnode.etl
logman stop iisnode -ets 2>&1 > nul
logman start iisnode -ow -p {1040DFC4-61DB-484A-9530-584B2735F7F7} -o %ETL% -ets 2>&1 > nul
if "%ERRORLEVEL%" neq "0" echo Error starting ETW listener for iisnode. & exit /b -1
echo Started ETW listener for iisnode. Execute your test scenario, then press a key to stop the listener and see the traces.
pause
logman stop iisnode -ets 2>&1 > nul
if "%ERRORLEVEL%" neq "0" echo Error stopping ETW listener for iisnode. & exit /b -1
del /q %ETL%.xml 2>&1 > nul
tracerpt %ETL% -o %ETL%.xml 2>&1 > nul
if "%ERRORLEVEL%" neq "0" echo Error generating trace report from %ETL% & exit /b -1
echo Traces are at %ETL%
echo The report is at %ETL%.xml
echo Press a key to see the report in your default *.xml viewer, or Ctrl-C to exit.
pause
start %ETL%.xml
endlocal