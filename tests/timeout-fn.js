'use strict';
let ivm = require('isolated-vm');

(async function() {
	let isolate = new ivm.Isolate;
	let context = isolate.createContextSync();
	let global = context.globalReference();
	global.setSync('global', global.derefInto());
	isolate.compileScriptSync('global.run = () => { for(;;); }').runSync(context);
	let run = global.getSync('run');
	let uhoh = false;
	setTimeout(function() {
		uhoh = true;
	}, 500);
	try {
		run.applySync(undefined, [], { timeout: 20 });
		console.log('what?');
	} catch (err) {}

	try {
		await run.apply(undefined, [], { timeout: 20 });
		console.log('what?');
	} catch (err) {
		console.log(uhoh ? 'uhoh' : 'pass');
	}
})().catch(console.error);
