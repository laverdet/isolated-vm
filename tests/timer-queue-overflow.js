'use strict';
let ivm = require('isolated-vm');

// https://github.com/laverdet/isolated-vm/issues/572
let isolate = new ivm.Isolate({ memoryLimit: 128 });
let context = isolate.createContextSync();
let fn = isolate.compileScriptSync('(function(x) { return x + 1; })').runSync(context, { reference: true });

let horizon = Date.now() + 10000;
let armed = 0;
for (let ii = 0; ii < 100000; ++ii) {
	let timeout = horizon - Date.now();
	if (timeout < 1000) break;
	fn.applySync(undefined, [ii], {
		timeout,
		arguments: { copy: true },
		result: { copy: true },
	});
	++armed;
}

if (armed < 5000) {
	console.log('armed only ' + armed + ' timers');
} else {
	setTimeout(function() {
		console.log('pass');
	}, horizon - Date.now() + 2000);
}
