"use strict";
const ivm = require('isolated-vm');
const assert = require('assert');

function v8AtLeast(major, minor, revision) {
	let v8 = process.versions.v8.split(/\./g);
	return v8[0] > major ||
		(v8[0] == major && v8[1] >= minor) ||
		(v8[0] == major && v8[1] == minor && v8[2] >= revision);
}

let snapshot = ivm.Isolate.createSnapshot([ { code:
`
	const array = new Uint32Array(128);
	array[100] = 0xdeadbeef;
	function sum(a,b) { return a + b }
`
} ]);
let isolate = new ivm.Isolate({ snapshot });
try {
	snapshot.copy({ transferIn: true });
	assert.fail('buffer transferred');
} catch (err) {
	assert.ok(/in use/.test(err), 'weird error');
}
let context = isolate.createContextSync();
assert.equal(isolate.compileScriptSync('sum(1, 2)').runSync(context), 3);
if (v8AtLeast(6, 2, 193)) {
	assert.equal(isolate.compileScriptSync('array[100]').runSync(context), 0xdeadbeef);
}
console.log('pass');
