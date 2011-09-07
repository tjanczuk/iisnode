setlocal 

set IISNODETEST_PORT=80

call %this%scripts\runNodeTest.bat parts\200_samples.js

endlocal