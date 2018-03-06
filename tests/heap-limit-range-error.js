'use strict';
let ivm = require('isolated-vm');

function bisect(fn, ii) {
	// Phase 1, search upwards
	let min = ii;
	let max = ii * 2;
	while (!fn(max)) {
		max *= 2;
	}

	// Phase 2, binary search
	do {
		let mid = (min + max) >> 1;
		if (fn(mid)) {
			if (mid - 1 <= min) {
				return mid;
			}
			max = mid - 1;
		} else {
			if (mid + 1 >= max) {
				return mid;
			}
			min = mid + 1;
		}
	} while (true);
}

// Detect isolate failure limit
let memoryLimit = 16;
let failure = bisect(function(ii) {
	let isolate = new ivm.Isolate({ memoryLimit });
	let context = isolate.createContextSync();
	isolate.compileScriptSync(''+ function load(num) {
		let store = [];
		while (num--) {
			store.push({});
		}
	}).runSync(context);
	try {
		context.global.getSync('load').applySync(null, [ ii ]);
		return false;
	} catch (err) {
		return true;
	}
}, 100000);

// Try to kill an isolate with an array buffer
let overflow = Array(1024).fill().map(() => {});
overflow.push(new ArrayBuffer(1024));
let isolate = new ivm.Isolate({ memoryLimit });
let context = isolate.createContextSync();
isolate.compileScriptSync(''+ function loadAndCopy(num, copy) {
	let store = [];
	while (num--) {
		store.push({});
	}
	copy.copy();
}).runSync(context);
try {
	context.global.getSync('loadAndCopy').applySync(null, [ failure - 1024, new ivm.ExternalCopy(overflow) ]);
	console.log('did not fail?');
} catch (err) {
	if (err instanceof RangeError) {
		console.log('pass');
	} else {
		console.log('got error', err);
	}
}
