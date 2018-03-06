'use strict';
let ivm = require('isolated-vm');

let copies = [
	new ivm.ExternalCopy(Array(1024 * 25.5).fill().map(() => 'aa')), // 1mb strings
	new ivm.ExternalCopy(Array(1024 * 128).fill().map(() => {})), // 1mb objects
	new ivm.ExternalCopy(Array(1024 * 25.5 * 8).fill().map(() => 'aa')), // 8mb strings
	new ivm.ExternalCopy(Array(1024 * 128 * 8).fill().map(() => {})), // 8mb objects
	new ivm.ExternalCopy(Array(1024 * 25.5 * 32).fill().map(() => 'aa')), // 32mb strings
	new ivm.ExternalCopy(Array(1024 * 128 * 32).fill().map(() => {})), // 32mb objects
];

for (let memoryLimit of [ 8, 32, 64, 128 ]) {
	for (let copy of copies) {
		let isolate = new ivm.Isolate({ memoryLimit });
		let context = isolate.createContextSync();
		let global = context.global;
		try {
			for (let ii = 0; ii < 256; ++ii) {
				global.setSync(ii, copy.copyInto());
			}
		} catch (err) {}
	}
}

console.log('pass');
