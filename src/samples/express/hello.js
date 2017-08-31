var express = require('express');

var app = express.createServer();

app.get('*/myapp/foo', function (req, res) {
    res.send('Hello from foo! [express sample]');
});

app.get('*/myapp/bar', function (req, res) {
    res.send('Hello from bar! [express sample]');
});

app.listen(process.env.PORT);