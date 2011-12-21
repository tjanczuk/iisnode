var http = require('http');

var server = http.createServer(function (req, res) {
    res.writeHead(200, {'Content-Type': 'text/html'});
    res.end('Hello, world!');
});

if (!process.env.mode) {
    server.listen(process.env.PORT);
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
        server.listen(31416);
    }
}
