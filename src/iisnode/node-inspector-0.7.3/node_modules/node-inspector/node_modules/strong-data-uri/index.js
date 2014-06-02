/* Copyright (c) 2013 StrongLoop, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
'use strict';

var truncate = require('truncate');

/**
 * Decode payload in the given data URI, return the result as a Buffer.
 * See [RFC2397](http://www.ietf.org/rfc/rfc2397.txt) for the specification
 * of data URL scheme.
 * @param {String} uri
 * @returns {Buffer}
 */
function decode(uri) {
  if (!/^data:/.test(uri))
    throw new Error('Not a data URI: "' + truncate(uri, 20) + '"');

  var commaIndex = uri.indexOf(',');
  if (commaIndex < 0)
    throw new Error('Invalid data URI "' + truncate(uri, 20) + '"');

  var header = uri.slice(0, commaIndex);
  var body = uri.slice(commaIndex+1);

  if (/;base64$/.test(header)) {
    return new Buffer(body, 'base64');
  } else {
    return new Buffer(decodeURIComponent(body), 'ascii');
  }
}

exports.decode = decode;
