'use strict';
let ivm = require('isolated-vm');
let assert = require('assert');

{
	assert.doesNotThrow(() => {
		new ivm.ExternalCopy(new Error('hi'));
	});
}

{
	assert.doesNotThrow(() => {
		let object = {};
		Object.defineProperty(object, 'property', { enumerable: true, get() { return 'hi'; } });
		new ivm.ExternalCopy(object);
	});
}

console.log('pass');
