var http = require('http');

http.createServer(function (req, res) {
    res.writeHead(400, {'Content-Type': 'text/html'});
    res.end('App created error. Gets replaced by IIS Custom error if skipIISCustomErrors="false"');
}).listen(process.env.PORT);  