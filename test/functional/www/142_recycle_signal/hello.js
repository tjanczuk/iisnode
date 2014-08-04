var http = require('http');
var net = require('net');

http.createServer(function (req, res) {
    res.setHeader('processpid', process.pid);
    res.writeHead(200, {'Content-Type': 'text/html'});
    res.end('Hello, world!');
    setTimeout(function() { 
        var stream = net.connect(process.env.IISNODE_CONTROL_PIPE);
        stream.write('recycle');
        stream.end();
      }, 3000);
}).listen(process.env.PORT);  