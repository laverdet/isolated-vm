'use strict';
let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;
function check(name, fn) {
	try {
		fn();
	} catch (err) {
		if (err.message.indexOf(name) === -1) {
			console.log(err);
			console.log('fail', name);
			throw new Error;
		}
	}
}
[
	[ 'test1', () => isolate.compileScriptSync('**', { filename: 'test1.js' }) ],
	[ 'test2', () => ivm.Isolate.createSnapshot([ { code: '**', filename: 'test2.js' } ]) ],
	[ 'isolated-vm', () => ivm.Isolate.createSnapshot([ { code: '1' } ], '**') ],
	[ 'test3', () => isolate.compileModuleSync('**', { filename: 'test3.js' }) ],
].map(val => check(val[0], val[1]));
console.log('pass');
