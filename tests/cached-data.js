'use strict';
let ivm = require('isolated-vm');
let src = Array(20000).fill('function a(){}').join(';');

// Each isolate seems to have some internal source cache so each of these tests run in a new
// isolate instead of reusing the same one for multiple tests

{
	// Try compilation with no flags and ensure nothing extra is returned
	let isolate = new ivm.Isolate;
	let script = isolate.compileScriptSync(src);
	if (script.cachedDataRejected !== undefined || script.cachedData !== undefined) {
		console.log('fail1');
	}
}

{
	// Try broken cache data
	let isolate = new ivm.Isolate;
	let script = isolate.compileScriptSync(src, { cachedData: new ivm.ExternalCopy(Buffer.from('garbage').buffer) });
	if (script.cachedDataRejected !== true) {
		console.log('fail2');
	}
}

let cachedData;
{
	// Produce some cached data
	let isolate = new ivm.Isolate;
	let script = isolate.compileScriptSync(src, { produceCachedData: true });
	cachedData = script.cachedData;
	if (cachedData === undefined) {
		console.log('fail3');
	}
}

{
	// Time compilation with no cached data
	let d = Date.now();
	let timeout = 200;
	let count = 0;
	do {
		let isolate = new ivm.Isolate;
		let script = isolate.compileScriptSync(src);
		++count;
	} while (Date.now() < d + timeout);

	// Compare to compilation with cached data
	d = Date.now();
	for (let ii = 0; ii < count; ++ii) {
		let isolate = new ivm.Isolate;
		let script = isolate.compileScriptSync(src, { cachedData });
		if (script.cachedDataRejected !== false) {
			console.log('fail4');
		}
	}
	if (Date.now() - d > timeout / 2) {
		console.log('cached data is suspiciously slow');
	}
}

console.log('pass');
