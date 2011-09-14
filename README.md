Hosting node.js applications in IIS on Windows
===

**Prerequisites for using**

- Windows
- IIS 7.x with IIS Management Tools
- [URL rewrite module for IIS](http://www.iis.net/download/URLRewrite)
- Latest node.exe Windows build from [nodejs.org](http://nodejs.org/#download) saved to %systemdrive%\node directory
- Visual C++ 2010 Redistributable Package for [x86](http://www.microsoft.com/download/en/details.aspx?id=5555) or [x64](http://www.microsoft.com/download/en/details.aspx?id=14632) (skip this if you install Visual Studio)
- your favorite text editor; [WebMatrix](http://www.microsoft.com/web/webmatrix/) is recommended

**Installing for IIS 7.x from a download**

- [Download and run the MSI for 32 or 64 bit Windows](https://github.com/tjanczuk/iisnode/archives/master)

**Installing for IIS Express/WebMatrix**

- [Check out the walkthhrough](http://tomasz.janczuk.org/2011/08/developing-nodejs-applications-in.html)

**Samples**

- Install for IIS 7.x (see previous sections)
- From the administrative command prompt call `%programfiles%\iisnode\setupsamples.bat`  
- Go to `http://localhost/node`

**Howtos**

[the basics](http://tomasz.janczuk.org/2011/08/hosting-nodejs-applications-in-iis-on.html)  
[using with express framework](http://tomasz.janczuk.org/2011/08/hosting-express-nodejs-applications-in.html)  
[using with URL rewrite module](http://tomasz.janczuk.org/2011/08/using-url-rewriting-with-nodejs.html)  
[using with WebMatrix and IIS Express](http://tomasz.janczuk.org/2011/08/developing-nodejs-applications-in.html)  
[site templates for WebMatrix](https://github.com/SteveSanderson/Node.js-Site-Templates-for-WebMatrix)  
[using with mongodb](http://www.amazedsaint.com/2011/09/creating-10-minute-todo-listing-app-on.html)  

**Prerequisites for building**

- All prerequisities for using
- [Visual Studio C++ Express](http://www.microsoft.com/visualstudio/en-us/products/2010-editions/visual-cpp-express)
- [WIX Toolset v3.5](http://wix.codeplex.com/releases/view/60102)
- [IIS 7 header files from Windows SDK](http://msdn.microsoft.com/en-us/windows/bb980924)

**Building**

For 32 bit Windows:

    msbuild /p:Platform=Win32 src\iisnode\iisnode.sln

For 64 bit Windows:

    msbuild /p:Platform=x64 src\iisnode\iisnode.sln

**Installing for IIS 7.x after build**

    build\debug\%PROCESSOR_ARCHITECTURE%\iisnode.msi
    
**Running tests**

- Install for IIS 7.x (see previous sections)
- `test\functional\test.bat`

**Resources & documentation**

https://github.com/tjanczuk/iisnode/wiki  
http://tomasz.janczuk.org