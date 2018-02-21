'use strict';
let ivm = require('isolated-vm');

let foo = {};
let bar = 'bar';
let fnRef = new ivm.Reference(function(a, b) {
	if (a !== foo) {
		throw new Error('ref is wrong');
	}
	if (b !== bar) {
		throw new Error('copy is wrong');
	}
});

let ref = new ivm.Reference(foo);
let copy = new ivm.ExternalCopy(bar);

let tmp1 = ref.derefInto({ release: true });
let tmp2 = copy.copyInto({ release: true });

fnRef.applySync(undefined, [ tmp1, tmp2 ]);

try {
	fnRef.applySync(undefined, [ tmp1 ]);
	console.log('no error');
} catch (err) {
	if (!/only be used once/.test(err.message)) {
		throw err;
	}
}

try {
	fnRef.applySync(undefined, [ tmp2 ]);
	console.log('no error');
} catch (err) {
	if (!/only be used once/.test(err.message)) {
		throw err;
	}
}

try {
	ref.deref();
	console.log('no error');
} catch (err) {
	if (!/released/.test(err.message)) {
		throw err;
	}
}

try {
	copy.copy();
	console.log('no error');
} catch (err) {
	if (!/released/.test(err.message)) {
		throw err;
	}
}

console.log('pass');
