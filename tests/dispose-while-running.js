'use strict';
let ivm = require('isolated-vm');
(async function() {
	for (let ii = 0; ii < 100; ++ii) {
		let isolate = new ivm.Isolate;
		let script = await isolate.compileScript('for(;;);');
		let context = await isolate.createContext();
		setTimeout(function() {
			isolate.dispose();
		}, 1);
		try {
			await script.run(context);
		} catch (err) {
			// if this fails the process just locks up
		}
	}
	console.log('pass');
}()).catch(console.error);
