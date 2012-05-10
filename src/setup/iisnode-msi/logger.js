(function () {

	var path = require('path')
		, fs = require('fs');

	// maximum total size of log files

	var maxlogsize = process.env.IISNODE_MAX_DISTRIBUTED_LOG_SIZE || (2 * 1024 * 1024); 

	// polyfill node v0.7 fs.existsSync with node v0.6 path.existsSync

	var existsSync = fs.existsSync || path.existsSync;

	// refactor process.argv to determine the app entry point and remove the interceptor

	var appFile;
	var newArgs = [];
	process.argv.forEach(function (item, index) {
		if (index === 2)
			appFile = item;
		if (index !== 1)
			newArgs.push(item);
	});

	process.argv = newArgs;

	// determine the root of the Antares log file directory

	var wwwroot = path.dirname(appFile);
	var logdir = path.resolve(wwwroot, '../../LogFiles/nodejs'); 

	if (!existsSync(logdir)) {
		try {
			fs.mkdirSync(logdir);
		}
		catch (e) {

			// check if directory was created in the meantime (by another instance)

			if (!existsSync(logdir))
				throw e;
		}
	}

	// purge old logs if total size exceeds quota

	var files = fs.readdirSync(logdir);
	var stats = [];
	var totalsize = 0;
	
	files.forEach(function (file) {
		var stat = fs.statSync(path.resolve(logdir, file));
		if (stat.isFile()) {
			stats.push(stat);
			stat.file = file;
			totalsize += stat.size;
		}
	});

	if (totalsize > maxlogsize) {
		// keep deleting files from the least recently modified to the most recently modified until
		// the total size of log files gets within the quota

		stats.sort(function (a, b) {
			return a.mtime.getTime() - b.mtime.getTime();
		});

		stats.some(function (stat) {
			try {
				fs.unlinkSync(path.resolve(logdir, stat.file));
				totalsize -= stat.size;
			}
			catch (e) {
				// likely indicates the file is still in use; leave it alone
			}

			return totalsize < maxlogsize;
		});
	}

	// capture current time for log file names

	var now = Date.now();

	// intercept stdout and stderr

	var intercept = function (stream, type) {
		var filename = process.env.COMPUTERNAME + '-' + process.pid + '-' + type + '-' + now + '.txt';
		var dest = fs.createWriteStream(path.resolve(logdir, filename));

		var oldEnd = stream.end;
		stream.end = function () {
			dest.end.apply(dest, arguments);
			oldEnd.apply(stream, arguments);
		};

		var oldWrite = stream.write;
		stream.write = function () {
			dest.write.apply(dest, arguments);
			oldWrite.apply(stream, arguments);
		}
	};

	intercept(process.stdout, 'stdout');
	intercept(process.stderr, 'stderr');

	// run the original application entry point

	require(appFile);
})();