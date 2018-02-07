'use strict';
let ivm = require('isolated-vm');
let arr = [ 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 ];
let str = '1,2,3,4,5,6,7,8';

// When you do this, HasBuffer() is false for some reason
{
	let copy = new ivm.ExternalCopy(new Uint8Array(arr));
	if (copy.copy().join() !== str) {
		console.log('fail1');
	}
}

// Test transfer out
{
	let buffer = (new Uint8Array(arr)).buffer;
	let view = new Uint8Array(buffer);
	let copy = new ivm.ExternalCopy(buffer, { transfer: true });
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
	let buffer3 = copy.copy({ transfer: true });
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
	let copy2 = new ivm.ExternalCopy(buffer3, { transfer: true });
	try {
		view3.join();
		console.log('fail6');
	} catch (err) {}

	// Transfer in one last time
	let buffer4 = copy2.copy();
	let view4 = new Uint8Array(buffer4);
	if (view4.join() !== str) {
		console.log('fail7');
	}

	// Transfer into an isolate
	let isolate = new ivm.Isolate;
	let global = isolate.createContextSync().globalReference();
	global.setSync('foo', copy2.copyInto({ transfer: true }));
	try {
		copy2.copy();
	} catch (err) {}
}

// Try typed array
{
	let view = new Uint8Array(arr);
	let copy = new ivm.ExternalCopy(view, { transfer: true });
	try {
		view.join();
		console.log('fail8');
	} catch (err) {}
	if (copy.copy({ transfer: true }).join() !== str) {
		console.log('fail9');
	}
	try {
		copy.copy();
		console.log('fail10');
	} catch (err) {}

}

console.log('pass');
