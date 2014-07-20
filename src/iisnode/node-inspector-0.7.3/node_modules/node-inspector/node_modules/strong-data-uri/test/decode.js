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

var expect = require('chai').expect;

var dataUri = require('..');

describe('decode', function() {
  it('parses base64-encoded payload into a buffer', function() {
    var buffer = dataUri.decode('data:text/plain;base64,aGVsbG8gd29ybGQ=');
    expect(buffer).to.be.instanceOf(Buffer);
    expect(buffer.toString('ASCII')).to.equal('hello world');
  });

  it('parses url-encoded payload into a buffer', function() {
    var buffer = dataUri.decode('data:,hello%20world');
    expect(buffer.toString('ASCII')).to.equal('hello world');
  });

  it('throws when URI does not start with "data:"', function() {
    expect(function() { dataUri.decode('http://foo/bar'); })
      .to.throw(/^Not a data URI/);
  });

  it('throws when URI does not contain comma', function() {
    expect(function() { dataUri.decode('data:text/plain'); })
      .to.throw(/^Invalid data URI/);
  });
});

