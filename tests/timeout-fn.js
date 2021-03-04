'use strict';
let ivm = require('isolated-vm');

(async function() {
	let isolate = new ivm.Isolate;
	let context = isolate.createContextSync();
	let global = context.global;
	global.setSync('global', global.derefInto());
	isolate.compileScriptSync('global.run = () => { function the_stack() { for(;;); }; the_stack(); }').runSync(context);
	let run = global.getSync('run', { reference: true });
	let uhoh = false;
	let timeout = setTimeout(function() {
		uhoh = true;
	}, 500);
	try {
		run.applySync(undefined, [], { timeout: 20 });
	} catch (err) {
		if (!/the_stack/.test(err.stack)) {
			console.log('missing stack');
		}
	}

	try {
		await run.apply(undefined, [], { timeout: 20 });
		console.log('what?');
	} catch (err) {
		if (!/the_stack/.test(err.stack)) {
			console.log('missing stack');
		}
		console.log(uhoh ? 'uhoh' : 'pass');
		clearTimeout(timeout);
	}
})().catch(console.error);
