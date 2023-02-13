const ivm = require('isolated-vm');
const assert = require('assert');

const code = `
	function loopFn(${new Array(2048).fill(0).map((_, idx) => 'arg' + idx).join(',')}) {
		let i = 0;
		let result = 0;
		for (i = 0; i < arguments.length; i++) {
			result += arguments[i];
		}
		return result;
	}

	const foo = {};

	for (let i = 2048; i > 0; i--) {
		loopFn.bind(foo).call(new Array(i).fill(0).map((_, idx) => { 
			if (i % 2 === 0) {
				return idx + Math.random(i);
			} else {
				return idx + String(i);
			}
		}));
	}
`

const checkProfile = (profile, idx) => {
	assert(typeof profile.startTime === 'number', `profiles[${idx}].startTime should be a number`);
	assert(typeof profile.endTime === 'number', `profile[${idx}].startTime should be a number`);
	assert(Array.isArray(profile.samples), `profile[${idx}].samples should be an array`);
	assert(Array.isArray(profile.timeDeltas), `profile[${idx}].timeDeltas should be an array`);
	assert(Array.isArray(profile.nodes), `profile[${idx}].timeDeltas should be an array`);
};

const testSync = async () => {
	const isolate = new ivm.Isolate();

	isolate.startCpuProfiler('test');

	const context = isolate.createContextSync();
	context.evalSync(code, {
		filename: 'foo.js'
	});
	context.release();

	const context2 = isolate.createContextSync();
	context2.evalSync(code, {
		filename: 'bar.js'
	});
	context2.release();

	const profiles = await isolate.stopCpuProfiler('test');

	assert.ok(profiles.length > 0, 'profiles should have length > 0');

	profiles.forEach(({ threadId, profile }, idx) => {
		assert.ok(typeof threadId === 'number', 'threadId should be a number');
		checkProfile(profile, idx);
	});

	const strProfiles = JSON.stringify(profiles);
	assert.ok(strProfiles.includes('loopFn'), 'loopFn should be in the result');
	assert.ok(strProfiles.includes('foo.js'), 'foo.js filename should be in the result');
	assert.ok(strProfiles.includes('bar.js'), 'bar.js filename should be in the result');
	isolate.dispose();
}

const testAsync = async () => {
	const isolate = new ivm.Isolate();

	isolate.startCpuProfiler('test');

	const runOne = async (filename) => {
		const context = await isolate.createContext();
		await context.eval(code, {
			filename,
		});
		context.release();
	}


	await Promise.all([
		runOne('foo.js'),
		runOne('bar.js'),
		runOne('baz.js'),
		runOne('boo.js'),
		runOne('loo.js'),
		runOne('yoo.js'),
	]);

	const profiles = await isolate.stopCpuProfiler('test');

	assert.ok(profiles.length > 0, 'profiles should have length > 0');

	profiles.forEach(({ threadId, profile }, idx) => {
		assert.ok(typeof threadId === 'number', 'threadId should be a number');
		checkProfile(profile, idx);
	});

	const strProfiles = JSON.stringify(profiles, null, 2);

	assert.ok(strProfiles.includes('loopFn'), 'loopFn should be in the result');
	assert.ok(strProfiles.includes('foo.js'), 'foo.js filename should be in the result');
	assert.ok(strProfiles.includes('bar.js'), 'bar.js filename should be in the result');
	isolate.dispose();
};

const testEmpty = async () => {
	const isolate = new ivm.Isolate();
	const profiles = await isolate.stopCpuProfiler('test');
	assert.ok(profiles.length === 0, 'profiles should have length 0');
};


Promise.all([
	testEmpty(),
	testSync(),
	testAsync(),
]).then(() => {
	console.log('pass');
});
