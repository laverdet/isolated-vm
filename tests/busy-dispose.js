'use strict';
let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;
let task = isolate.createContext();
try {
	isolate.dispose();
} catch (err) {}
task.catch(function() {
	try {
		isolate.dispose();
	} catch (err) {
		console.log('pass');
	}
});
