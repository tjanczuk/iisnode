var express = require('express');
var app = express.createServer();

var products = [
    { name: "Performance of IIS for dummies, Vol 1.", price: 34.12 },
    { name: "Performance of IIS for dummies, Vol 2.", price: 35.12 },
    { name: "Performance of IIS for dummies, Vol 3.", price: 36.12 },
    { name: "Introduction to express.js, Vol 1.", price: 27.50 },
    { name: "Introduction to express.js, Vol 2.", price: 28.50 },
    { name: "Introduction to express.js, Vol 3.", price: 29.50 },
    { name: "Node.js for Ruby users, Vol 1.", price: 7.50 },
    { name: "Node.js for Ruby users, Vol 2.", price: 8.50 },
    { name: "Node.js for Ruby users, Vol 3.", price: 9.50 },
    { name: "Node.js for PHP users, Vol 1.", price: 4.50 },
    { name: "Node.js for PHP users, Vol 2.", price: 5.50 },
    { name: "Node.js for PHP users, Vol 3.", price: 6.50 },
    { name: "Node.js for .NET users, Vol 1.", price: 13.50 },
    { name: "Node.js for .NET users, Vol 2.", price: 14.50 },
    { name: "Node.js for .NET users, Vol 3.", price: 15.50 },
];

app.configure(function() {
    app.set('view engine', 'jade');
    app.set('views', __dirname + '/views');
});

app.get('/order', function(req, res) {
    var order = { details: [], subtotal: 0 };

    for (var i = 0; i < 4; i++) {
        var id = Math.floor(Math.random() * products.length);
        order.details.push(products[id]);
        order.subtotal += products[id].price;
    }    

    order.handling = Math.round(Math.random() * 1000) / 100;
    order.tax = Math.round((order.subtotal + order.handling) * 8.5) / 100;
    order.total = order.subtotal + order.handling + order.tax;
    order.id = Math.floor(Math.random() * 1000000000);
    order.layout = false;

    res.render('order', order);
});

if (!process.env.mode) {
    app.listen(process.env.PORT);
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
        app.listen(31416);
    }
}
