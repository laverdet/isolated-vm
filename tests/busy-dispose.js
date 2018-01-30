'use strict';
let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;
let task = isolate.createContext();
isolate.dispose();
function done() {
	try {
		isolate.dispose();
	} catch (err) {
		console.log('pass');
	}
}
task.then(done);
task.catch(done);
