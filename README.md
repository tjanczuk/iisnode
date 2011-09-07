Hosting node.js applications in IIS on Windows
===

**Prerequisites**

- Windows
- IIS 7.x with IIS Management Tools
- [URL rewrite module for IIS](http://www.iis.net/download/URLRewrite) if you don't have it already installed
- Latest node.exe Windows build from [nodejs.org](http://nodejs.org/#download) saved to %systemdrive%\node directory
- One of the following:
  - [Visual Studio C++ Express](http://www.microsoft.com/visualstudio/en-us/products/2010-editions/visual-cpp-express) (if you intend to build iisnode in addition to using it)
  - Visual C++ 2010 Redistributable Package for [x86](http://www.microsoft.com/download/en/details.aspx?id=5555) or [x64](http://www.microsoft.com/download/en/details.aspx?id=14632) (if you only intend to use iisnode)
- [IIS 7 header files from Windows SDK](http://msdn.microsoft.com/en-us/windows/bb980924) (building iisnode only)
- your favorite text editor; [WebMatrix](http://www.microsoft.com/web/webmatrix/) is recommended (developing node.js apps on Windows only)

**Building**

For 32 bit Windows:

    msbuild /p:Platform=Win32 src\iisnode\iisnode.sln

For 64 bit Windows:

    msbuild /p:Platform=x64 src\iisnode\iisnode.sln

**Installing for IIS 7.x after build**

    build\debug\%PROCESSOR_ARCHITECTURE%\install.bat

**Installing for IIS 7.x from a download**

- [Download and unzip desired build for 32 or 64 bit Windows](https://github.com/tjanczuk/iisnode/archives/master)
- call install.bat

**Installing for IIS Express/WebMatrix**

- [Check out the walkthhrough](http://tomasz.janczuk.org/2011/08/developing-nodejs-applications-in.html)

**Samples**

    http://localhost/node
    
**Running tests**

Install for IIS 7.x (see previous sections), then:  

    test\functional\test.bat

**Howtos**

[the basics](http://tomasz.janczuk.org/2011/08/hosting-nodejs-applications-in-iis-on.html)  
[using with express framework](http://tomasz.janczuk.org/2011/08/hosting-express-nodejs-applications-in.html)  
[using with URL rewrite module](http://tomasz.janczuk.org/2011/08/using-url-rewriting-with-nodejs.html)  
[using with WebMatrix and IIS Express](http://tomasz.janczuk.org/2011/08/developing-nodejs-applications-in.html)  
[using with mongodb](http://www.amazedsaint.com/2011/09/creating-10-minute-todo-listing-app-on.html)  

**Resources & documentation**

https://github.com/tjanczuk/iisnode/wiki
http://tomasz.janczuk.org