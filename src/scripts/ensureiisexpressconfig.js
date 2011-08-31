// This ensures that user-scope IIS Express applicationhost.config file is created at
// %userprofile%\documents\iisexpress\config\applicationHost.config

try {
    var versionMgr = new ActiveXObject("Microsoft.IIS.VersionManager");
    WScript.Echo('Obtained Microsoft.IIS.VersionManager');
    var iisex = versionMgr.GetVersionObject("7.5", 2);
    WScript.Echo('Obtained version object 7.5');
    var userObj = iisex.GetPropertyValue("userInstanceHelper");
    WScript.Echo('Obtained userInstanceHelper');
    userObj.SetupIISDirectory(false);
    WScript.Echo('Set up IIS directory');
    WScript.Quit(0);
} catch (e) {
    WScript.Echo('Exception');
    WScript.Quit(1);
}