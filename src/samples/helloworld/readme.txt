This is the helloworld sample showing basic hosting of a node.js application in IIS. 

Key points:

1. An IIS handler for node.js must be registered in web.config to designate the hello.js
   file as a node.js application. This allows other *.js files in this web application
   to be served by IIS as static client side JavaScript files. 

2. The node.js listen address must be specified as 'process.env.PORT' when starting the
   listener. When an application is hosted in IIS, the web server controls the base address
   of the application, which is provided to the node process via an environment variable. 