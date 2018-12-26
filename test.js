#!/usr/bin/env node
let fs = require('fs');
let spawn = require('child_process').spawn;
let path = require('path');

let ret = 0;
let passCount = 0, failCount = 0;
function runTest(test, cb) {
	// Copy env variables
	let env = {};
	for (let ii in process.env) {
		env[ii] = process.env[ii];
	}
	env.NODE_PATH = __dirname;

	// Get extra args
	let args = [];
	let testPath = path.join('tests', test);
	let content = fs.readFileSync(testPath, 'utf8');
	let match = /node-args: *(.+)/.exec(content);
	if (match) {
		args = match[1].split(/ /g);
	}
	args.push(testPath);

	// Launch process
	let proc = spawn(process.execPath, args, { env });
	proc.stdout.setEncoding('utf8');
	proc.stderr.setEncoding('utf8');

	let stdout = '', stderr = '';
	proc.stdout.on('data', function(data) {
		stdout += data;
	});
	proc.stderr.on('data', function(data) {
		stderr += data;
	});
	proc.stdin.end();

	// Wait for completion
	process.stderr.write(`${test}: `);
	proc.on('exit', function(code) {
		if (stdout !== 'pass\n' || stderr !== '') {
			++failCount;
			ret = 1;
			console.error(
				`*fail*\n`+
				`code: ${code}\n`+
				`stderr: ${stderr}\n`+
				`stdout: ${stdout}`
			);
		} else if (code !== 0) {
			++failCount;
			ret = 1;
			console.error(`fail (${code})`);
		} else {
			++passCount;
			console.log(`pass`);
		}
		cb();
	});
}

let cb = function() {
	console.log('\nCompleted: '+ passCount+ ' passed, '+ failCount+ ' failed.');
	process.exit(ret);
};
fs.readdirSync('./tests').sort().reverse().forEach(function(file) {
	if (/\.js$/.test(file)) {
		cb = new function(cb) {
			return function(err) {
				if (err) return cb(err);
				runTest(file, cb);
			};
		}(cb);
	}
});
cb();
