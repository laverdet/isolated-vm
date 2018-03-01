'use strict';
let ivm = require('isolated-vm');
let fs = require('fs');

let isolate = new ivm.Isolate;
let context = isolate.createContextSync();
let global = context.globalReference();
global.setSync('fn', new ivm.Reference(function() {
	return fs.readFileSync(__filename, 'utf8');
}));
isolate.compileScriptSync(''+ function recursive() {
	throw new Error('passed the test');
}).runSync(context);
let recursiveFn = global.getSync('recursive');

(async function() {
	let script = await isolate.compileScript('fn.applySync(undefined, [])');
	let value = await script.run(context);
	if (/hello123/.test(value)) {
		try {
			console.log(recursiveFn.applySync(undefined, []));
		} catch (err) {
			if (/passed the test/.test(err)) {
				console.log('pass');
			}
		}
	}
})().catch(console.error);
