var http = require('http');
var file = new(require('node-static').Server)(__dirname + '/public');

var app = http.createServer(function (request, response) {
    request.addListener('end', function() {
        file.serve(request, response);
    });
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
