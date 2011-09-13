// Removes iisnode configuration section from system.webServer section group in
// %systemroot%\system32\inetsrv\config\applicationHost.config (IIS)

// http://www.ksingla.net/2007/02/using_ahadmin_to_read_write_iis_configuration_part_2/

function main() {
    var ahwrite = new ActiveXObject("Microsoft.ApplicationHost.WritableAdminManager");
    var configManager = ahwrite.ConfigManager;
    var appHostConfig = configManager.GetConfigFile("MACHINE/WEBROOT/APPHOST");
    var systemWebServer = appHostConfig.RootSectionGroup.Item("system.webServer");
    try {
        systemWebServer.Sections.DeleteSection("iisnode");
    }
    catch (e) {
        // nothing to remove
    }
    ahwrite.CommitChanges();
}