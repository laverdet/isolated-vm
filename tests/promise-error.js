let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;
let context = isolate.createContextSync();
(async function() {
	try {
		isolate.compileScriptSync('Promise.resolve().then(() => { throw new Error("hello") })').runSync(context);
		console.log('fail1');
	} catch (err) {
		if (!/hello/.test(err.message)) {
			console.log('fail2');
		}
	}

	try {
		await (await isolate.compileScript('Promise.resolve().then(() => { throw new Error("hello") })')).run(context);
		console.log('fail3');
	} catch (err) {
		if (!/hello/.test(err.message)) {
			console.log('fail4');
		}
	}

	console.log('pass');
})().catch(console.error);
