var http = require('http');
var fs = require('fs');
var path = require('path');
var n = 1;

var cases = {
    empty: '', // success
    noKey1: ': foo', // failure
    noKey2: '   : bar', // failure
    emptyValue1: 'foo:', // success
    emptyValue2: 'foo:    # comment', // success
    unrecognizedKey: 'foo: bar\nbaz: 12', // success
    invalidBool: 'loggingEnabled: foobar', // failure
    emptyLine: 'foo:bar\n\n\nbaz:7' // success
};

var nodeConfig = path.resolve(__dirname, 'iisnode.yml');

http.createServer(function (req, res) {
    res.writeHead(200, { 'Content-Type': 'text/html' });
    var query = require('url').parse(req.url).query;

    if (typeof cases[query] === 'string') {
        fs.writeFileSync(nodeConfig, cases[query]);
        res.end('iisnode.yml updated');
    }
    else {
        res.end('Hello, world ' + n++);
    }
}).listen(process.env.PORT);  