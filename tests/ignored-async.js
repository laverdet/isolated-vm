'use strict';
let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;
(async function() {
	let context = await isolate.createContext();
	let script = await isolate.compileScript('foo + 1 == bar ? "pass" : "fail"');
	let global = context.global;
	if (global.setIgnored('foo', 1) || global.setIgnored('bar', 2)) {
		console.log('fail');
	}
	console.log(await script.run(context));
}()).catch(console.error);

