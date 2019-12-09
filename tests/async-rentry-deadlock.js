'use strict';
let ivm = require('isolated-vm');

function makeIsolate() {
	let isolate = new ivm.Isolate;
	let context = isolate.createContextSync();
	let global = context.global;
	return { isolate, context, global };
}

{
	// Basic test.. tests unlocking
	let env = makeIsolate();
	env.global.setSync('fn', new ivm.Reference(function() {}));
	env.isolate.compileScriptSync('fn.applySync(undefined, []);').runSync(env.context);

	let d = Date.now();
	while (Date.now() < d + 5);
	env.global.setSync('dummy', 1);
}

console.log('pass');
