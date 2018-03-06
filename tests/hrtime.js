// Create a new isolate limited to 128MB
let ivm = require('isolated-vm');
let isolate = new ivm.Isolate;
let context = isolate.createContextSync();
let jail = context.global;
jail.setSync('lib', ivm.lib);
isolate.compileScriptSync('new '+ function() {
	let time1 = lib.hrtime();
	let time2 = lib.hrtime();
	if (time1[0] === time2[0]) {
		if (time1[1] >= time2[1]) {
			throw new Error('nanoseconds wrong');
		}
	} else if (time1[0] > time2[0]) {
		throw new Error('seconds wrong');
	}
	let diff = lib.hrtime(lib.hrtime());
	if (diff[0] !== 0 || diff[1] === 0) {
		throw new Error('diff doesnt work?');
	}
}).runSync(context);
let time1 = ivm.lib.hrtime();
let time2 = process.hrtime();
let diff = (time2[0] * 1e9 + time2[1]) - (time1[0] * 1e9 + time1[1]);
if (diff <= 0 || diff > 5000000) { // 5ms
	console.log('doesnt match process.hrtime', diff);
}
console.log('pass');

