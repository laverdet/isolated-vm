'use strict';
let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;
let task = isolate.createContext();
try {
	isolate.dispose();
	console.log('disposed?');
} catch (err) {}
task.then(function() {
	isolate.dispose();
	console.log('pass');
});
