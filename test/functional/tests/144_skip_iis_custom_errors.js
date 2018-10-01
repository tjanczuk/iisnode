var iisnodeassert = require("iisnodeassert");

iisnodeassert.sequence([
    // when skipIISCustomErrors="true", error responses returned by the node application should pass through to the client
    // when skipIISCustomErrors="false", IIS can intercept error responses from the node application with custom error handling based on the error code
    // this option only comes into play when httpErrors existingResponse="Auto".
    // existingResponse="PassThrough" and existingResponse="Replace" will always pass error messages from the node app through to the client or replace them, respectively
    iisnodeassert.get(10000, "/143_skip_iis_custom_errors/on/create_error.js", 400, 'App created error. Gets replaced by IIS Custom error if skipIISCustomErrors="false"'),
    iisnodeassert.get(10000, "/143_skip_iis_custom_errors/off/create_error.js", 400, 'Bad Request'),

    // iisnode skipIISCustomErrors option should not affect custom error routing for errors outside of iisnode's purview
    iisnodeassert.get(10000, "/143_skip_iis_custom_errors/on/access_denied.js", 401, 'IIS Custom Error page still gets served for non iisnode error responses'),
    iisnodeassert.get(10000, "/143_skip_iis_custom_errors/off/access_denied.js", 401, 'IIS Custom Error page still gets served for non iisnode error responses'),

    // internal iisnode errors should still get extra handling by IIS depending on error mode
    iisnodeassert.get(10000, "/143_skip_iis_custom_errors/on/malformed.js", 500, 'The page cannot be displayed because an internal server error has occurred.'),
    iisnodeassert.get(10000, "/143_skip_iis_custom_errors/off/malformed.js", 500, 'The page cannot be displayed because an internal server error has occurred.'),    
]);