"use strict";
let ivm = require('isolated-vm');
let snapshot = ivm.Isolate.createSnapshot([ { code: 'function sum(a,b) { return a + b }' } ]);
let isolate = new ivm.Isolate({ snapshot });
let context = isolate.createContextSync();
console.log(isolate.compileScriptSync('sum(1, 2)').runSync(context) === 3 ? 'pass' : 'fail');
