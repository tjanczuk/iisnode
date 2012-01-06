var http = require('http')
    , version = require('part').partVersion;

http.createServer(function (req, res) {
    res.writeHead(200, {'Content-Type': 'text/html'});
    res.end('Hello, world ' + version);
}).listen(process.env.PORT);  