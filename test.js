#!/usr/bin/env node
let fs = require('fs');
let spawn = require('child_process').spawn;
let path = require('path');

let ret = 0;
function runTest(test, cb) {
	let env = {};
	for (let ii in process.env) {
		env[ii] = process.env[ii];
	}
	env.NODE_PATH = __dirname;
	let proc = spawn(
		process.execPath,
		[ path.join('tests', test) ],
		{ env: env }
	);
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

	process.stderr.write(`${test}: `);
	proc.on('exit', function(code) {
		if (stdout !== 'pass\n' || stderr !== '') {
			ret = 1;
			console.error(
				`*fail*\n`+
				`code: ${code}\n`+
				`stderr: ${stderr}\n`+
				`stdout: ${stdout}`
			);
		} else if (code !== 0) {
			ret = 1;
			console.error(`fail (${code})`);
		} else {
			console.log(`pass`);
		}
		cb();
	});
}

let cb = function() {
	process.exit(ret);
};
fs.readdirSync('./tests').reverse().forEach(function(file) {
	cb = new function(cb) {
		return function(err) {
			if (err) return cb(err);
			runTest(file, cb);
		};
	}(cb);
});
cb();
