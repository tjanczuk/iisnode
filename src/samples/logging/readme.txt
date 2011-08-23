This is the logging sample showing basic logging capabilities of node.js applications in IIS. 

Key points:

1. Output from console.log (or any stdout or stderr output) from a node.js application is
   captured, stored in a file on disk, and available through HTTP.

2. By default, given a node.js application at http://localhost/node/logging/hello.js, the logs
   are available at http://localhost/node/logging/hello.js.logs/0.txt. Logs are updated every 5 seconds
   and limited to 128KB in size. 

3. Check the configuration sample to customize the logging behavior.