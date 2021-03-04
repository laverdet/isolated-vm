'use strict';
let ivm = require('isolated-vm');
function makeIsolate() {
	let isolate = new ivm.Isolate({ memoryLimit: 16 });
	let context = isolate.createContextSync();
	let jail = context.global;
	jail.setSync('context', context);
	jail.setSync('isolate', isolate);
	jail.setSync('global', jail.derefInto());
	return { isolate, context, global };
}

(async function() {
	try {
		let env = makeIsolate();
		let script = await env.isolate.compileScript('new '+ function() {
			// Now we're inside the isolate
			let script2 = isolate.compileScriptSync('new '+ function() {
				let stash = [];
				for (;;) {
					stash.push({});
				}
			}).runSync(context);
		});
		await script.run(env.context);
		console.log('fail1');
	} catch (err) {
		if (!/disposed/.test(err)) throw err;
	}

	// Repeat but with applySync()
	try {
		let env = makeIsolate();
		let script = await env.isolate.compileScript('new '+ function() {
			// Now we're inside the isolate
			global.sabotage = function() {
				let stash = [];
				for (;;) {
					stash.push({});
				}
			}
			let ref = context.global.getSync('sabotage');
			ref();
		});
		await script.run(env.context);
		console.log('fail2');
	} catch (err) {
		if (!/disposed/.test(err)) throw err;
	}

	console.log('pass');

}()).catch(console.error);
