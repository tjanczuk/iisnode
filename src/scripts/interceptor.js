(function () {

    // refactor process.argv to determine the app entry point and remove the interceptor parameter

    var appFile;
    var newArgs = [];
    process.argv.forEach(function (item, index) {
        if (index === 2)
            appFile = item;
        if (index !== 1)
            newArgs.push(item);
    });

    process.argv = newArgs;

    // determine if logging is enabled

    if (process.env.IISNODE_LOGGINGENABLED != 1) {
        return;
    }

    var path = require('path')
		, fs = require('fs');

    // configuration

    var maxLogSize = (process.env.IISNODE_MAXLOGFILESIZEINKB || 128) * 1024;
    var maxTotalLogSize = (process.env.IISNODE_MAXTOTALLOGFILESIZEINKB || 1024) * 1024;
    var maxLogFiles = (+process.env.IISNODE_MAXLOGFILES) || 20;
    var relativeLogDirectory = process.env.IISNODE_logDirECTORY || 'iisnode';

    // polyfill node v0.7 fs.existsSync with node v0.6 path.existsSync

    var existsSync = fs.existsSync || path.existsSync;

    // determine the the abosolute log file directory

    var wwwroot = path.dirname(appFile);
    var logDir = path.resolve(wwwroot, relativeLogDirectory);

    // ensure the log directory structure exists

    var ensureDir = function (dir) {
        if (!existsSync(dir)) {

            ensureDir(path.dirname(dir));

            try {
                fs.mkdirSync(dir);
            }
            catch (e) {

                // check if directory was created in the meantime (by another process)

                if (!existsSync(dir))
                    throw e;
            }
        }
    };

    ensureDir(logDir);

    // generate index.html file

    var htmlTemplate = fs.readFileSync(path.resolve(__dirname, 'logs.template.html'), 'utf8');
    var indexFile = path.resolve(logDir, 'index.html');

    var updateIndexHtml = function () {
        var files = fs.readdirSync(logDir);
        var logFiles = [];

        files.forEach(function (file) {
            var match = file.match(/(.+)-(\d+)-(stderr|stdout)-(\d+)\.txt$/);
            if (match) {
                logFiles.push({
                    file: file,
                    computername: match[1],
                    pid: +match[2],
                    type: match[3],
                    created: +match[4]
                });
            }
        });

        var html = htmlTemplate.replace('[]; //##LOGFILES##', JSON.stringify(logFiles)).replace('0; //##LASTUPDATED##', new Date().getTime());

        try {
            fs.writeFileSync(indexFile, html);
        }
        catch (e) {
            // empty - might be a collistion with concurrent update of index.html from another process
        }
    };

    // make best effort to purge old logs if total size or file count exceeds quota

    var purgeOldLogs = function () {
        var files = fs.readdirSync(logDir);
        var stats = [];
        var totalsize = 0;

        files.forEach(function (file) {
            if (file !== 'index.html') {
                try {
                    var stat = fs.statSync(path.resolve(logDir, file));
                    if (stat.isFile()) {
                        stats.push(stat);
                        stat.file = file;
                        totalsize += stat.size;
                    }
                }
                catch (e) {
                    // empty - file might have been deleted by other process
                }
            }
        });

        if (totalsize > maxTotalLogSize || stats.length > maxLogFiles) {

            // keep deleting files from the least recently modified to the most recently modified until
            // the total size and number of log files gets within the respective quotas

            stats.sort(function (a, b) {
                return a.mtime.getTime() - b.mtime.getTime();
            });

            var totalCount = stats.length;

            stats.some(function (stat) {
                try {
                    fs.unlinkSync(path.resolve(logDir, stat.file));
                    totalsize -= stat.size;
                    totalCount--;
                }
                catch (e) {
                    // likely indicates the file is still in use; leave it alone
                }

                return totalsize <= maxTotalLogSize && totalCount <= maxLogFiles;
            });
        }
    };

    // intercept a stream

    var intercept = function (stream, type) {

        var currentLog;
        var currentSize;
        var currentLogCreated;

        var rolloverLog = function () {
            var now = new Date().getTime();
            var filename = process.env.COMPUTERNAME + '-' + process.pid + '-' + type + '-' + now + '.txt';
            currentLog = path.resolve(logDir, filename);
            currentSize = 0;
            currentLogCreated = false;
            purgeOldLogs();
        };

        rolloverLog(); // create a new log file

        var ensureBuffer = function (data, encoding) {
            if (Buffer.isBuffer(data)) {
                return data;
            }
            else {
                data = data.toString().replace(/\n/g, '\r\n');
                return new Buffer(data, typeof encoding === 'string' ? encoding : 'utf8');
            }
        };

        stream.write = stream.end = function (data, encoding) {
            var buffer = ensureBuffer(data, encoding);
            if (currentSize > maxLogSize) {
                rolloverLog();
            }

            if (!currentLogCreated) {
                fs.writeFileSync(currentLog, '', 'utf8');
                updateIndexHtml();
                currentLogCreated = true;
            }

            var f = fs.openSync(currentLog, 'a');
            currentSize += fs.writeSync(f, buffer, 0, buffer.length, currentSize);
            fs.closeSync(f);
        };
    };

    // intercept stdout and stderr

    intercept(process.stdout, 'stdout');
    intercept(process.stderr, 'stderr');

    // install uncaughtException handler such that we can trace the unhandled exception to stderr

    process.on('uncaughtException', function (e) {
        // only act on the uncaught exception if the app has not registered another handler
        if (1 === process.listeners('uncaughtException').length) {
            console.error('Application has thrown an uncaught exception and is terminated:\n' + (e.stack || (new Error(e).stack)));
            process.exit(1);
        }
    });

})();

// run the original application entry point

require(process.argv[1]);