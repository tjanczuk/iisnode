# strong-data-uri

[![Build Status](https://travis-ci.org/strongloop/strong-data-uri.png?branch=master)](https://travis-ci.org/strongloop/strong-data-uri)
[![NPM version](https://badge.fury.io/js/strong-data-uri.png)](http://badge.fury.io/js/strong-data-uri)

## Overview
strong-data-uri is implements a parser for retrieving data encoded
in `data:` URIs specified by [RFC2397](http://www.ietf.org/rfc/rfc2397.txt).

## Usage

```js
var dataUri = require('strong-data-uri');
var uri = 'data:text/plain;base64,aGVsbG8gd29ybGQ=';

var buffer = dataUri.decode(uri);
console.log(buffer);
// <Buffer 68 65 6c 6c 6f 20 77 6f 72 6c 64>
console.log(buffer.toString('ascii'));
// Hello world
```

## Status

The module can parse both base64-encoded and url-encoded URLs at the moment.

Things that would be nice to have too:

 * Parse mediaType information and extract charset (encoding) value. This is
   needed to convert the returned Buffer into a string in cases where
   the application has to support arbitrary encodings.

 * Implement `encode()` function for creating data URLs.
