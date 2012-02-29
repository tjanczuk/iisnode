// Removes iisnode configuration section from system.webServer section group in
// %userprofile%\documents\iisexpress\config\applicationHost.config (IIS Express)

function main() {
    try {
        var versionMgr = new ActiveXObject("Microsoft.IIS.VersionManager");
        var iisex = versionMgr.GetVersionObject("7.5", 2);
        var ahwrite = iisex.CreateObjectFromProgId("Microsoft.ApplicationHost.WritableAdminManager");
        var configManager = ahwrite.ConfigManager;
        var appHostConfig = configManager.GetConfigFile("MACHINE/WEBROOT/APPHOST");
        var systemWebServer = appHostConfig.RootSectionGroup.Item("system.webServer");
        systemWebServer.Sections.DeleteSection("iisnode");
        ahwrite.CommitChanges();
    }
    catch (e) {
        // nothing to remove or IIS Express had been uninstalled before iisnode
    }
}