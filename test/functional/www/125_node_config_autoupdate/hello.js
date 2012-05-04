var http = require('http');

var n = 1;

http.createServer(function (req, res) {
    res.writeHead(200, { 'Content-Type': 'text/html' });
    var query = require('url').parse(req.url).query;
    if (query === 'update') {
        require('fs').writeFileSync(require('path').resolve(__dirname, 'node.config'), 'nodeProcessCountPerApplication: 4');
        res.end('node.config updated');
    }
    else if (query === 'revert') {
        require('fs').writeFileSync(require('path').resolve(__dirname, 'node.config'), 'nodeProcessCountPerApplication: 2');
        res.end('node.config reverted');
    }
    else {
        res.end('Hello, world ' + n++);
    }
}).listen(process.env.PORT);  