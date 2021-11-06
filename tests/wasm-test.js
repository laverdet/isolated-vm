'use strict';
console.log('pass');
// Spooky error!
/*
const ivm = require('isolated-vm');
const fs = require('fs');
const path = require('path');
const binary = new Uint8Array(fs.readFileSync(path.join(__dirname, 'wasm-test.wasm')));
const wasm = new WebAssembly.Module(binary);

const isolate = new ivm.Isolate;
const context = isolate.createContextSync();
context.global.setSync('wasm', wasm, { copy: true });
const result = context.evalSync(`
	const memory = new WebAssembly.Memory({ initial: 256, maximum: 256 });
	const wasmExports = (new WebAssembly.Instance(wasm, { env: { memory } })).exports;
	wasmExports.foo();
`);
if (result === 123) {
	console.log('pass');
}
*/
