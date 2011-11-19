Performance tests for iisnode require hosting of a node.js application in IIS 7.x. 
Performance tests depend on the WCAT tool that must be installed separately on the client machine:

  x86: http://www.iis.net/community/default.aspx?tabid=34&g=6&i=1466
  x64: http://www.iis.net/community/default.aspx?tabid=34&g=6&i=1467

All scripts assume WCAT has been installed in the default location of %programfiles%\WCAT.

Server.bat is used to start a server to test performance of using either vanilla node.exe or iisnode. 

Client.bat is used to launch WCAT against that server and measure its performance under load. 

Client.bat and Server.bat are intended to be run on different machines. For functional validation
purposes or light stress testing they can be run on the same machine using LocalRun.bat.

Performance tests are organized in "scenarios". Each scenario consists of a node.js application to test, 
configuration settings for iisnode (web.config), and client side WCAT scenario plan. Each scenario is stored in a 
separate subdirectory of the www directory. That subdirectory name is the scenario name. Client.bat and Server.bat
take scenario name as a parameter and can run only one scenario at a time. 

