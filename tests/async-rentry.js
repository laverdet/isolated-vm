'use strict';
let ivm = require('isolated-vm');
let fs = require('fs');

let isolate = new ivm.Isolate;
let context = isolate.createContextSync();
let global = context.global;
let fnPromiseRef;
global.setSync('fnPromise', fnPromiseRef = new ivm.Reference(function() {
	return new Promise(function(resolve, reject) {
		fs.readFile(__filename, 'utf8', function(err, val) {
			err ? reject(err) : resolve(val);
		});
	});
}));
global.setSync('fnSync', new ivm.Reference(function() {
	return 'hello';
}));

try {
	fnPromiseRef.applySyncPromise(undefined, []);
	console.log('applySyncPromise is not allowed');
} catch (err) {
	if (!/default thread/.test(err)) {
		console.log('strange error');
	}
}

isolate.compileScriptSync(`function recursive() {
	throw new Error('passed the test');
}`).runSync(context);
let recursiveFn = global.getSync('recursive');

(async function() {
	let script = await isolate.compileScript('fnPromise.applySyncPromise(undefined, [], {})');
	let value = await script.run(context);
	if (/hello123/.test(value)) {
		try {
			console.log(recursiveFn());
		} catch (err) {
			if (/passed the test/.test(err)) {
				console.log('pass');
			}
		}
	}

	let script2 = await isolate.compileScript('fnSync.applySync(undefined, [])');
	if (await script2.run(context) !== 'hello') {
		console.log('nevermind it didnt pass');
	}
})().catch(console.error);

// Test slow promise timeout
global.setSync('slowPromise', new ivm.Reference(function() {
	return new Promise(function(resolve) {
		setTimeout(function() {
			resolve();
		}, 50);
	});
}));
isolate.compileScriptSync('slowPromise.applySyncPromise(undefined, [])').run(context, { timeout: 1 }).catch(() => 0);

// Test dead promise (This causes a memory leak! Don't do this!)
// Disabled test because `timeout` is now paused when the isolate is not active.
/*
global.setSync('deadPromise', new ivm.Reference(function() {
	return new Promise(() => {});
}));
isolate.compileScriptSync('deadPromise.applySyncPromise(undefined, [])').run(context, { timeout: 1 }).catch(() => 0);
*/
