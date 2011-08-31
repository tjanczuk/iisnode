// Installs iisnode configuration section into system.webServer section group in
// %userprofile%\documents\iisexpress\config\applicationHost.config

// TODO, tjanczuk, find a programmatic way of registering iisnode section in IIS Express configuration 

try {
    var fso = new ActiveXObject("Scripting.FileSystemObject");
    WScript.Echo("Created FileSystemObject");
    var f = fso.OpenTextFile(WScript.Arguments.Item(0), 1);
    WScript.Echo("Opened file " + WScript.Arguments.Item(0) + " for reading");
    var config = f.ReadAll();
    WScript.Echo("Read all content");
    f.Close();
    WScript.Echo("Closed file");

    if (!config.match(/\<\s*section\s*name\s*=\s*"iisnode"\s*\/s*>/)) {
        WScript.Echo("iisnode section registration not found");
        config = config.replace(/(\<\s*sectionGroup\s*name\s*=\s*"system.webServer"\s*>)/, '$1<section name="iisnode"/>');
        WScript.Echo("added iisnode section registration");
    }
    else {
        WScript.Echo("existing iisnode section registration found");
        WScript.Quit(0);
    }

    f = fso.OpenTextFile(WScript.Arguments.Item(0), 2, true);
    WScript.Echo("Open file for writing");
    f.Write(config);
    WScript.Echo("Written file");
    f.Close();
    WScript.Echo("Closed file");

    WScript.Quit(0);
} catch (e) {
    WScript.Echo("Exception");
    WScript.Quit(1);
}