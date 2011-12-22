var express = require('express');
var app = express.createServer();

app.configure(function() {
    if (process.env.mode) {
        // this is to make sure express does not serve static files when in IIS
    	app.use(express.static(__dirname + '/public'));
    }
});

if (!process.env.mode) {
    app.listen(process.env.PORT);
}
else {
    var cluster = require('cluster');

    if (cluster.isMaster) {
        for (var i = 0; i < 4; i++) {
            cluster.fork();
        }

        cluster.on('death', function(worker) {
            cluster.log('worker ' + worker.pid + ' died');
        });
    }
    else {
        app.listen(31416);
    }
}
