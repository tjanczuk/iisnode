var buffer = new Buffer(1024);
var numKB = 1024 * 30; // 30MB downlaod

require('http').createServer(function (req, res) {
    req.on('data', function (data) {} );
    req.on('end', function () {
        res.writeHead(200, {'Content-Type': 'application/binary', 'Cache-Control': 'no-cache'});
        for (var i = 0; i < numKB; i++) 
            res.write(buffer);
        res.end();
    });
}).listen(process.env.PORT || 3000);  