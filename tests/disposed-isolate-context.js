'use strict';
let ivm = require('isolated-vm');

let isolate = new ivm.Isolate;
let context = isolate.createContextSync();
let script = isolate.compileScriptSync('1');
isolate.dispose();
try {
	script.runSync(context);
} catch (err) {
	console.log('pass');
}
