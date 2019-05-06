const assert = require('assert');
const ivm = require('isolated-vm');
let isolate = new ivm.Isolate({ memoryLimit: 128 });
let context = isolate.createContextSync();
assert.throws(function() {
	isolate.compileScriptSync(`[...'.'.repeat(1e9)]; undefined`).runSync(context);
});
console.log('pass');
