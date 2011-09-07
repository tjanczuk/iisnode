:: run one node-based test
:: %1 - the fully qualified file name of the *.js file

%node% "%this%tests\%1" 2>"%this%tests\%1.err"
type "%this%tests\%1.err" 
for /F %%A IN ('dir /s /b "%this%tests\%1.err"') do set size=%%~zA
del /q "%this%tests\%1.err"
if "%size%" neq "0" exit /b -1
exit /b 0