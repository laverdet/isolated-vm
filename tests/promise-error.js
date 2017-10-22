let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;
let context = isolate.createContextSync();
let script = isolate.compileScriptSync('Promise.resolve().then(() => { throw new Error("hello") })');
try {
	script.runSync(context);
	console.log('fail');
} catch (err) {
	if (/hello/.test(err.message)) {
		console.log('pass');
	} else {
		console.log('fail');
	}
}
