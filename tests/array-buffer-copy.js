'use strict';
let ivm = require('isolated-vm');
let assert = require('assert');
let arr = [ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 ];
let str = '1,2,3,4,5,6,7,8';

// When you do this, HasBuffer() is false for some reason
{
	let copy = new ivm.ExternalCopy(new Uint8Array(arr));
	if (copy.copy().join() !== str) {
		console.log('fail1');
	}
}

{
	// ArrayBuffer::New will crash if the allocator fails
	let isolate = new ivm.Isolate({ memoryLimit: 16 });
	let array = new ivm.ExternalCopy(new Uint8Array(1024 * 1024 * 16));
	try {
		isolate.createContextSync().global.setSync('a', array.copyInto());
	} catch (err) {}
}

// Test transfer out
{
	let buffer = (new Uint8Array(arr)).buffer;
	let view = new Uint8Array(buffer);
	let copy = new ivm.ExternalCopy(buffer, { transferOut: true });
	try {
		view.join();
		console.log('fail2');
	} catch (err) {}

	// Copy in with no transfer
	let buffer2 = copy.copy();
	let view2 = new Uint8Array(buffer2);
	if (view2.join() !== str) {
		console.log('fail3');
	}

	// Transfer in
	let buffer3 = copy.copy({ transferIn: true });
	let view3 = new Uint8Array(buffer3);
	if (view3.join() !== str) {
		console.log('fail4');
	}

	// Confirm destroyed
	try {
		copy.copy();
		console.log('fail5');
	} catch (err) {}

	// Transfer out again (different code path)
	let copy2 = new ivm.ExternalCopy(buffer3, { transferOut: true });
	try {
		view3.join();
		console.log('fail6');
	} catch (err) {}

	// Test transfer list
	assert.doesNotThrow(() => {
		let view = new Uint8Array(arr);
		let copy = new ivm.ExternalCopy(view, { transferList: [ view.buffer ] });
		assert.throws(() => view.join(), /(?:neutered|detached) ArrayBuffer/); // has been released
		assert.strictEqual(copy.copy().join(), str);
	});

	assert.doesNotThrow(() => {
		let view = new Uint8Array(arr);
		let ext = new ivm.ExternalCopy({ buffer: view.buffer, view }, { transferList: [ view.buffer ] });
		let copy1 = ext.copy();
		assert.strictEqual(copy1.buffer, copy1.view.buffer);
		assert.strictEqual(copy1.view.join(), str);
		let copy2 = ext.copy({ transferIn: true });
		assert.strictEqual(copy2.buffer, copy2.view.buffer);
		assert.strictEqual(copy2.view.join(), str);
		assert.throws(() => ext.copy(), /Array buffer is invalid/);
	});

	// Test sliced array
	assert.doesNotThrow(() => {
		let view = new Uint8Array(arr);
		let view2 = new Uint8Array(view.buffer, 4);
		let copy = new ivm.ExternalCopy(view2).copy();
		assert.ok(copy.length == arr.length - 4);
		assert.strictEqual(new Uint8Array(copy.buffer).join(), str);
	});

	// Transfer in one last time
	let buffer4 = copy2.copy();
	let view4 = new Uint8Array(buffer4);
	if (view4.join() !== str) {
		console.log('fail7');
	}

	// Transfer into an isolate
	let isolate = new ivm.Isolate;
	let global = isolate.createContextSync().global;
	global.setSync('foo', copy2.copyInto({ transferIn: true }));
	try {
		copy2.copy();
	} catch (err) {}
}

// Try typed array
{
	let view = new Uint8Array(arr);
	let copy = new ivm.ExternalCopy(view, { transferOut: true });
	try {
		view.join();
		console.log('fail8');
	} catch (err) {}
	if (copy.copy({ transferIn: true }).join() !== str) {
		console.log('fail9');
	}
	try {
		copy.copy();
		console.log('fail10');
	} catch (err) {}

}

console.log('pass');
