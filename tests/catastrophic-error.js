'use strict';
// Uhh this one you kind of have to just test yourself. Comment out the `process.exit` and check
// activity monitor to see total CPU
const ivm = require('isolated-vm');
console.log('pass');
process.exit();

for (const run of [
	context => context.eval('ivm.lib.testHang()', { timeout: 100 }),
	context => context.eval('ivm.lib.testOOM()'),
]) {
	const isolate = new ivm.Isolate({
		memoryLimit: 128,
		onCatastrophicError: message => {
			console.log(message);
		},
	});
	const context = isolate.createContextSync();
	context.global.setSync('ivm', ivm);
	run(context);
}
