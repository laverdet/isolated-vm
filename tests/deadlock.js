// Create a new isolate limited to 128MB
let ivm = require('isolated-vm');

let isolates = Array(2).fill().map(function() {
	let isolate = new ivm.Isolate;
	let context = isolate.createContextSync();
	let jail = context.global;
	jail.setSync('global', jail.derefInto());
	isolate.compileScriptSync('new '+ function() {
		global.random = function() {
			return Math.random();
		};
		global.hammer = function(fnRef) {
			for (let ii = 0; ii < 100; ++ii) {
				fnRef.applySync(undefined, []);
			}
		};
	}).runSync(context);
	return { isolate, jail };
});

let timeout;
(async function() {
	timeout = setTimeout(function() {
		console.log('test is deadlocked please kill');
		process.exit(1);
		setTimeout(function() {
			// This doesn't seem to work??
			process.kill(process.pid, 'SIGKILL');
		}, 100);
	}, 1000);
	await Promise.all([
		isolates[0].jail.getSync('hammer').apply(undefined, [ isolates[1].jail.getSync('random') ]),
		isolates[1].jail.getSync('hammer').apply(undefined, [ isolates[0].jail.getSync('random') ]),
	]);
	console.log('strange pass?');
})().catch(function() {
	console.log('pass');
	clearTimeout(timeout);
});
