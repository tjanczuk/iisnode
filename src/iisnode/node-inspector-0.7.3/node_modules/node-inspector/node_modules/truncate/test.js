var truncate = require('./truncate');

exports['should truncate text with given length'] = function(t){
    var input, expect, actual;

   input = null;
   actual = truncate(input, 3);
   expect = '';
   t.strictEqual(expect, actual);

   input = '';
   actual = truncate(input, 3);
   expect = '';
   t.strictEqual(expect, actual);

   input = 'hello';
   actual = truncate(input, 3);
   expect = 'hel...';
   t.strictEqual(expect, actual);

   input = 'I am not sure what I am talking about';
   actual = truncate(input, 10);
   expect = 'I am not s...';
   t.strictEqual(expect, actual);
    t.done();
};

exports['should keep url safe'] = function(t){
    var input, expect, actual;
    input = 'Hey http://distilleryimage8.s3.amazonaws.com/719bf2329ddd11e28c3122000aa80097_7.jpg';
    actual = truncate(input, 4);
    expect = 'Hey ...';
    t.strictEqual(expect, actual);

    input = 'Hey http://distilleryimage8.s3.amazonaws.com/719bf2329ddd11e28c3122000aa80097_7.jpg';
    actual = truncate(input, 5);
    expect = 'Hey http://distilleryimage8.s3.amazonaws.com/719bf2329ddd11e28c3122000aa80097_7.jpg';
    t.strictEqual(expect, actual);

    input  = 'http://successfulsoftware.net/2013/03/19/the-brutal-truth-about-marketing-your-software-product/ http://successfulsoftware.net/2013/03/19/the-brutal-truth-about-marketing-your-software-product2/';
    actual = truncate(input, 96);
    expect = 'http://successfulsoftware.net/2013/03/19/the-brutal-truth-about-marketing-your-software-product/...';
    t.strictEqual(expect, actual);

    input  = 'http://successfulsoftware.net/2013/03/19/the-brutal-truth-about-marketing-your-software-product/ http://successfulsoftware.net/2013/03/19/the-brutal-truth-about-marketing-your-software-product2/';
    actual = truncate(input, 100);
    expect = 'http://successfulsoftware.net/2013/03/19/the-brutal-truth-about-marketing-your-software-product/ http://successfulsoftware.net/2013/03/19/the-brutal-truth-about-marketing-your-software-product2/';
    t.strictEqual(expect, actual);

    input = 'Hey http://distilleryimage8.s3.amazonaws.com/719bf2329ddd11e28c3122000aa80097_7.jpg';
    actual = truncate(input, 5);
    expect = 'Hey http://distilleryimage8.s3.amazonaws.com/719bf2329ddd11e28c3122000aa80097_7.jpg';
    t.strictEqual(expect, actual);

    input = 'Hey http://hehe.com plop http://plop.com';
    actual = truncate(input, 6);
    expect = 'Hey http://hehe.com...';
    t.strictEqual(expect, actual);

    input = 'Hey http://hehe.com plop http://plop.com';
    actual = truncate(input, 22);
    expect = 'Hey http://hehe.com pl...';
    t.strictEqual(expect, actual);

    input = 'Hey http://hehe.com plop http://plop.com';
    actual = truncate(input, 25);
    expect = 'Hey http://hehe.com plop ...';
    t.strictEqual(expect, actual);

    input = 'Hey http://hehe.com plop http://plop.com';
    actual = truncate(input, 26);
    t.strictEqual(input, actual);

    t.done();
};
