rem When a *.js file that is a dependency of the main entry point to the application is modified, application is recycled

setlocal 

copy /y %www%\117_autoupdate_dependency\hello_first.js %www%\117_autoupdate_dependency\node_modules\part.js
if %ERRORLEVEL% neq 0 exit /b -1

call %this%scripts\runNodeTest.bat parts\117_autoupdate_dependency_first.js
if %ERRORLEVEL% neq 0 del /q %www%\117_autoupdate_dependency\node_modules\part.js & exit /b -1

copy /y %www%\117_autoupdate_dependency\hello_second.js %www%\117_autoupdate_dependency\node_modules\part.js
if %ERRORLEVEL% neq 0 exit /b -1

timeout /T 5 /NOBREAK

call %this%scripts\runNodeTest.bat parts\117_autoupdate_dependency_second.js
if %ERRORLEVEL% neq 0 del /q %www%\117_autoupdate_dependency\node_modules\part.js & exit /b -1

del /q %www%\117_autoupdate_dependency\node_modules\part.js

exit /b 0

endlocal