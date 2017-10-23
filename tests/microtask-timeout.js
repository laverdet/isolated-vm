'use strict';
let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;
(async function() {
	let context = await isolate.createContext();
	let script = await isolate.compileScript(`
function bad() {
	Promise.resolve().then(function() {
		bad();
		for(;;);
	});
}
bad();
for(;;);
`);
	try {
		await script.run(context, { timeout: 100 });
	} catch (err) {
		if (!/timed out/.test(err.message)) {
			throw err;
		}
	}
	// This flushes the microtask queue so if any remain from above this would spin forever
	await isolate.createContext();
	console.log('pass');
})().catch(console.error);
