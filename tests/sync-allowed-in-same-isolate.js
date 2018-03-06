// Create a new isolate limited to 128MB
let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;

let context = isolate.createContextSync();
let jail = context.global;
jail.setSync('context', context);
jail.setSync('isolate', isolate);
isolate.compileScriptSync('new '+ function() {
	if (isolate.compileScriptSync('1').runSync(context) !== 1) {
		throw new Error;
	}
}).runSync(context);
console.log('pass');

