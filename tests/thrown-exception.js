const ivm = require('isolated-vm');
const cp = require('child_process');
const arg = '--the-arg';
if (process.argv[2] === arg) {
	const isolate = new ivm.Isolate;
	isolate.compileScriptSync('*');
} else {
	cp.execFile(process.execPath, [ __filename, arg ], { stdio: 'ignore' }, function(err, stdout, stderr) {
		if (err.code !== 1 || err.signal || err.killed) {
			console.log(err);
		} else {
			console.log('pass');
		}
	});
}
