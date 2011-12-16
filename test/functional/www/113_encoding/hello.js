var http = require('http');

http.createServer(function (req, res) {
    var query = require('url').parse(req.url).query;
    if (query === 'onechunk') { // chunked transfer encoding, one chunk
        res.writeHead(200, { 'Content-Type': 'text/html' });
        res.end('chunked response');
    }
    else if (query === 'tenchunks') { // chunked transfer encoding, ten chunks
        res.writeHead(200, { 'Content-Type': 'text/html' });
        var n = 0;
        function writeOne() {
            if (n < 9) {
                res.write(n.toString());
                n++;
                setTimeout(writeOne, 200);
            }
            else {
                res.end(n.toString());
            }
        }
        writeOne();
    }
    else { // fixed response length with Content-Length
        res.writeHead(200, { 'Content-Type': 'text/html', 'Content-Length': '23' });
        res.end('content-length response');
    }
}).listen(process.env.PORT);  