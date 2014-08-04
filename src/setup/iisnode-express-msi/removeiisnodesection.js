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
    
    try
    {
        var versionMgr = new ActiveXObject("Microsoft.ApplicationHost.WritableAdminManager");
        var iisex = versionMgr.GetVersionObject("7.5", 2);
        var ahwrite = iisex.CreateObjectFromProgId("Microsoft.ApplicationHost.WritableAdminManager");
        var traceFailedRequestsSection = ahwrite.GetAdminSection("system.webServer/tracing/traceFailedRequests", "MACHINE/WEBROOT/APPHOST");

        var traceFailedRequestsCollection = traceFailedRequestsSection.Collection;

        var addElementPos = FindElement(traceFailedRequestsCollection, "add", ["path", "*"]);
        if (addElementPos == -1) throw "Element not found!";
        var addElement = traceFailedRequestsCollection.Item(addElementPos);


        var traceAreasCollection = addElement.ChildElements.Item("traceAreas").Collection;

        var addElement1Pos = FindElement(traceAreasCollection, "add", ["provider", "WWW Server"]);
        if (addElement1Pos == -1) throw "Element not found!";
        var addElement1 = traceAreasCollection.Item(addElement1Pos);

        addElement1.Properties.Item("areas").Value = "Authentication,Security,Filter,StaticFile,CGI,Compression,Cache,RequestNotifications,Module,FastCGI,WebSocket,Rewrite,RequestRouting";

        ahwrite.CommitChanges();
    }
    catch(e) {
    }   
    
    try 
    {
        var versionMgr = new ActiveXObject("Microsoft.ApplicationHost.WritableAdminManager");
        var iisex = versionMgr.GetVersionObject("7.5", 2);
        var ahwrite = iisex.CreateObjectFromProgId("Microsoft.ApplicationHost.WritableAdminManager");
        ahwrite.CommitPath = "MACHINE/WEBROOT/APPHOST";

        var traceProviderDefinitionsSection = ahwrite.GetAdminSection("system.webServer/tracing/traceProviderDefinitions", "MACHINE/WEBROOT/APPHOST");

        var traceProviderDefinitionsCollection = traceProviderDefinitionsSection.Collection;

        var addElementPos = FindElement(traceProviderDefinitionsCollection, "add", ["name", "WWW Server"]);
        if (addElementPos == -1) throw "Element not found!";
        var addElement = traceProviderDefinitionsCollection.Item(addElementPos);

        var areasCollection = addElement.ChildElements.Item("areas").Collection;

        var addElement1Pos = FindElement(areasCollection, "add", ["name", "iisnode"]);
        if (addElement1Pos == -1) throw "Element not found!";

        areasCollection.DeleteElement(addElement1Pos);

        ahwrite.CommitChanges();
    }
    catch(e) {
    }    
}

function FindElement(collection, elementTagName, valuesToMatch) {
    for (var i = 0; i < collection.Count; i++) {
        var element = collection.Item(i);
        
        if (element.Name == elementTagName) {
            var matches = true;
            for (var iVal = 0; iVal < valuesToMatch.length; iVal += 2) {
                var property = element.GetPropertyByName(valuesToMatch[iVal]);
                var value = property.Value;
                if (value != null) {
                    value = value.toString();
                }
                if (value != valuesToMatch[iVal + 1]) {
                    matches = false;
                    break;
                }
            }
            if (matches) {
                return i;
            }
        }
    }
    
    return -1;
}