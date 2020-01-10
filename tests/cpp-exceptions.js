"use strict";
const ivm = require('isolated-vm');
const assert = require('assert');
assert.throws(() => ivm.ExternalCopy({}));
assert.throws(() => new ivm.Isolate('hello'));
console.log('pass');
