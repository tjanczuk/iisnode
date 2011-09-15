// This ensures that user-scope IIS Express applicationhost.config file is created at
// %userprofile%\documents\iisexpress\config\applicationHost.config

function main() {
    var versionMgr = new ActiveXObject("Microsoft.IIS.VersionManager");
    var iisex = versionMgr.GetVersionObject("7.5", 2);
    var userObj = iisex.GetPropertyValue("userInstanceHelper");
    userObj.SetupIISDirectory(false);
}