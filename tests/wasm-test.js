'use strict';
const ivm = require('isolated-vm');
const fs = require('fs');
const path = require('path');
let binary = fs.readFileSync(path.join(__dirname, 'wasm-test.wasm'));
binary = new Uint8Array(binary);

let isolate = new ivm.Isolate;
let context = isolate.createContextSync();
context.global.setSync('binary', new ivm.ExternalCopy(binary).copyInto({ release: true }));
let result = isolate.compileScriptSync(`
	let memory = new WebAssembly.Memory({ initial: 256, maximum: 256 });
	let wasm = new WebAssembly.Module(binary);
	let wasmExports = (new WebAssembly.Instance(wasm, { env: { memory } })).exports;
	wasmExports.foo();
`).runSync(context);
if (result === 123) {
	console.log('pass');
}
