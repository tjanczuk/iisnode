Hosting node.js applications in IIS on Windows
===

**Branches**

- master: stable version.
- iisnode-dev: development branch.

**Why would I want to do it?**

[Benefits](https://github.com/tjanczuk/iisnode/wiki)

**Who uses iisnode?**

- [Microsoft azure - benefits](http://blogs.msdn.com/b/hanuk/archive/2012/05/05/top-benefits-of-running-node-js-on-windows-azure.aspx)
- [Microsoft azure - get started with node.js](http://azure.microsoft.com/en-us/develop/nodejs/)
- [appharbor.com](http://blog.appharbor.com/2012/01/19/announcing-node-js-support)
- [discountasp.net](http://discountasp.net/press/2012_06_12_free-webmatrix-v2-rc-hosting-with-nodejs.aspx)
- [arvixe.com](http://arvixe.com)
- [smarterasp.net](http://www.smarterasp.net/)
- [gearhost.com](http://gearhost.com/)
- [webecs.com](http://webecs.com/)

**Prerequisites for using**

- Windows Vista, Windows 7, Windows 8, Windows Server 2008, or Windows Server 2012
- IIS 7.x with IIS Management Tools and ASP.NET
- WebSocket functionality requires IIS 8.x on Windows 8 or Windows Server 2012
- [URL rewrite module for IIS](http://www.iis.net/download/URLRewrite)
- [Latest node.js build for Windows](http://go.microsoft.com/?linkid=9784334)

**Installing for IIS 7.x/8.x**

- Install iisnode for IIS 7.x/8.x: [x86](https://github.com/azure/iisnode/releases/download/v0.2.21/iisnode-full-v0.2.21-x86.msi) or [x64](https://github.com/azure/iisnode/releases/download/v0.2.21/iisnode-full-v0.2.21-x64.msi) - choose bitness matching your system
- To set up samples, from the administrative command prompt call `%programfiles%\iisnode\setupsamples.bat`
- Go to `http://localhost/node`

**Installing for IIS Express/WebMatrix**

- [Install WebMatrix using the Web Platform Installer](http://www.microsoft.com/web/webmatrix/)
- Open WebMatrix, choose “Site from folder”, enter %localappdata%\iisnode\www, start the site, and play with the iisnode samples, or
- Use node.js templates to get started quickly with an Express application or a skeleton Hello World

**Installing for IIS Express 8 on Windows x64**
This can be a head-scratcher since IIS Express 8 gives you both 32-bit and 64-bit versions (http://www.iis.net/learn/extensions/introduction-to-iis-express/iis-80-express-readme). You can either:
- Install the full x64 version, then in Visual Studio go to Tools > Options > Projects and Solutions > Web Projects > Use the 64 bit version of IIS Express. This way you have a single install for both IIS and IIS Express.
- Separately install iisnode express version (https://github.com/azure/iisnode/wiki/iisnode-releases).


**Howtos**
=======
- [the basics](http://tomasz.janczuk.org/2011/08/hosting-nodejs-applications-in-iis-on.html)
- [the basics (Pусский перевод)](http://softdroid.net/hosting-nodejs-applications-ru)
- [**NEW: websockets**] (http://tomasz.janczuk.org/2012/11/how-to-use-websockets-with-nodejs-apps.html)
- [using with express framework](http://tomasz.janczuk.org/2011/08/hosting-express-nodejs-applications-in.html)
- [using with URL rewrite module](http://tomasz.janczuk.org/2011/08/using-url-rewriting-with-nodejs.html)
- [using with WebMatrix and IIS Express](http://tomasz.janczuk.org/2011/08/developing-nodejs-applications-in.html)
- [site templates for WebMatrix](https://github.com/SteveSanderson/Node.js-Site-Templates-for-WebMatrix)
- [using with mongodb](http://www.amazedsaint.com/2011/09/creating-10-minute-todo-listing-app-on.html)
- [diagnosing problems with ETW traces](http://tomasz.janczuk.org/2011/09/using-event-tracing-for-windows-to.html)
- [using with MVC](http://weblogs.asp.net/jgalloway/archive/2011/10/26/using-node-js-in-an-asp-net-mvc-application-with-iisnode.aspx)
- [portuguese: node.js no windows: instalando o iisnode](http://vivina.com.br/nodejs-windows-parte-2)
- [integrated debugging](http://tomasz.janczuk.org/2011/11/debug-nodejs-applications-on-windows.html)
- [**NEW: integrated debugging with node-inspector v0.7.3**](http://www.ranjithr.com/?p=98)
- [pub/sub server using faye](http://weblogs.asp.net/cibrax/archive/2011/12/12/transform-your-iis-into-a-real-time-pub-sub-engine-with-faye-node.aspx)
- [appharbor uses iisnode](http://blog.appharbor.com/2012/01/19/announcing-node-js-support)

**Prerequisites for building**

- All prerequisities for using
- [Visual Studio Express 2012 for Windows Desktop](http://www.microsoft.com/visualstudio/eng/downloads)
- [WIX Toolset v3.6](http://wix.codeplex.com/releases/view/93929)
- [Windows SDK for Windows 8](http://msdn.microsoft.com/en-us/windows/desktop/hh852363)

**Building**

Build commands should be issued from the build environment set up with `"%programfiles(x86)%\Microsoft Visual Studio 11.0\Common7\Tools\VsDevCmd.bat"`, assuming default installation location of Visual Studio 2012 on x64 platform.

For x86 build:

```
msbuild /p:Platform=Win32 src\iisnode\iisnode.sln
```

For x64 build:

```
msbuild /p:Platform=x64 src\iisnode\iisnode.sln
```

**Installing after build**

- For IIS 7.x/8.0: `build\debug\{x64|x86}\iisnode-full.msi`
- For IIS Express 7.x: `build\debug\x86\iisnode-express.msi`

**Running tests**

- Install for IIS 7.x/8.x (see previous sections)
- `test\functional\test.bat`
- note that for the WebSocket tests to pass you must be running in IIS 8.x on Windows 8 or Windows Server 2012

**Resources & documentation**

- [Releases](https://github.com/azure/iisnode/wiki/iisnode-releases)
- [Wiki](https://github.com/tjanczuk/iisnode/wiki)
- [Blog](http://tomasz.janczuk.org)
