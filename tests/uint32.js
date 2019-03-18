"use strict";
const ivm = require('isolated-vm');
const assert = require('assert');
let isolate = new ivm.Isolate;
let context = isolate.createContextSync();
assert.equal(0xdeadbeef, isolate.compileScriptSync('0xdeadbeef').runSync(context));
console.log('pass');
