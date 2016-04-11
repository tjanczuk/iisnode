Truncate [![Build Status](https://drone.io/github.com/FGRibreau/node-truncate/status.png)](https://drone.io/github.com/FGRibreau/node-truncate/latest)
==================

Truncate text and keeps urls safe.

## NPM
Install the module with: `npm install truncate`

## Usage

```javascript
// Browser
String.truncate("1234 http://google.com hey :)", 2) === "12..."
```

```javascript
// NodeJS
> truncate = require('truncate');
> truncate("1234 http://google.com hey :)", 4);
"1234..."
> truncate("1234 http://google.com hey :)", 4, {ellipsis:null}); // or ellipsis:''
"1234"
> truncate("1234 http://google.com hey :)", 6);
"1234 http://google.com..."
> truncate("1234 http://google.com hey :)", 100);
"1234 http://google.com hey :)"
```

## [Changelog](/CHANGELOG.md)

## Donate
[Donate Bitcoins](https://coinbase.com/checkouts/fc3041b9d8116e0b98e7d243c4727a30)

## License
Copyright (c) 2014 Francois-Guillaume Ribreau
Licensed under the MIT license.
