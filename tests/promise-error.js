let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;
let context = isolate.createContextSync();
(async function() {
	try {
		isolate.compileScriptSync('Promise.resolve().then(() => { throw new Error("hello") })').runSync(context);
		console.log('fail');
	} catch (err) {
		if (!/hello/.test(err.message)) {
			console.log('fail');
		}
	}

	try {
		await (await isolate.compileScript('Promise.resolve().then(() => { throw new Error("hello") })')).run(context);
		console.log('fail');
	} catch (err) {
		if (!/hello/.test(err.message)) {
			console.log('fail');
		}
	}

	console.log('pass');
})().catch(console.error);
