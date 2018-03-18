'use strict';
let v8 = require('v8');
let ivm = require('isolated-vm');

// Get the error chain set up
let env = [ 0, 0, 0 ].map(function() {
	let isolate = new ivm.Isolate;
	let context = isolate.createContextSync();
	let global = context.global;
	return { isolate, context, global };
});

env[0].script = env[0].isolate.compileScriptSync('function env0() { throw new Error("here"); }; env0();');
env[1].global.setSync('nextScript', env[0].script);
env[1].global.setSync('nextContext', env[0].context);
env[1].script = env[1].isolate.compileScriptSync('function env1() { nextScript.runSync(nextContext) }; env1();');
env[2].global.setSync('nextScript', env[1].script);
env[2].global.setSync('nextContext', env[1].context);
env[2].script = env[2].isolate.compileScriptSync('function env2() { nextScript.runSync(nextContext) }; env2();');

// Checker
function checkError(err, sync) {
	if (!err.stack.split(/\n/g).slice(1).every(line => /^    at /.test(line))) {
		console.log('Has weird stack');
	}
	let i0 = err.stack.indexOf('env0');
	let i1 = err.stack.indexOf('env1');
	let i2 = err.stack.indexOf('env2');
	let i3 = err.stack.indexOf('exception-info.js');
	if (sync === true) {
		if (i0 == -1 || i1 == -1 || i2 == -1 || i3 == -1 || i0 > i1 || i1 > i2 || i2 > i3) {
			console.log('Out of order stack');
		}
	} else {
		if (i0 == -1 || i3 == -1 || i0 > i3) {
			console.log('Out of order stack');
		}
	}
}

// Try sync
try {
	env[2].script.runSync(env[2].context);
	console.log('no error');
} catch (err) {
	checkError(err, true);
}

// Try async
env[0].script.run(env[0].context).then(() => console.log('no stack')).catch(checkError).then(function() {

	// Try plain object error
	try {
		env[0].isolate.compileSync('throw {}');
		console.log('did not throw');
	} catch (err) {}

	// Try disposed
	env[0].isolate.dispose();
	env[0].isolate.compileScript('1').catch(function(err) {
		if (/\.js/.test(err.stack)) {
			console.log('pass');
		}
	});

});
