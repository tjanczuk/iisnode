require('http').createServer(function (req, res) {
    req.setEncoding('binary');
    var contentLength = 0;
    req.on('data', function (data) {
        contentLength += data.length;
    });
    req.on('end', function () {
        res.writeHead(200, {'Content-Type': 'text/plain', 'Cache-Control': 'no-cache'});
        res.end('' + contentLength);
    });
}).listen(process.env.PORT || 3000);  