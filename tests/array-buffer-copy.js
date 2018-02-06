'use strict';
let ivm = require('isolated-vm');

let copy = new ivm.ExternalCopy(new Uint8Array([
  0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
]));
if (copy.copy().join() !== '1,2,3,4,5,6,7,8') {
	console.log('fail');
}

console.log('pass');
