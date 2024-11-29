'use strict';
const ivm = require('isolated-vm');
const assert = require('assert');
const { V8_AT_LEAST } = require('./lib/v8-version');
const src = Array(20000).fill().map((_, ii) => `function a${ii}(){}`).join(';');

// Each isolate seems to have some internal source cache so each of these tests run in a new
// isolate instead of reusing the same one for multiple tests

{
	// Try compilation with no flags and ensure nothing extra is returned
	const isolate = new ivm.Isolate;
	const script = isolate.compileScriptSync(src);
	assert.strictEqual(script.cachedData, undefined);
	assert.strictEqual(script.cachedDataRejected, undefined);
}

{
	// Try broken cache data and produce fallback
	const isolate = new ivm.Isolate;
	const script = isolate.compileScriptSync(src, { cachedData: new ivm.ExternalCopy(Buffer.from('garbage').buffer), produceCachedData: true });
	assert.ok(script.cachedData);
	assert.strictEqual(script.cachedDataRejected, true);
}

let cachedData;
{
	// Produce some cached data
	const isolate = new ivm.Isolate;
	const script = isolate.compileScriptSync(src, { produceCachedData: true });
	assert.ok(script.cachedData);
	assert.strictEqual(script.cachedDataRejected, undefined);
	cachedData = script.cachedData;

}

{
	const count = 3; // larger number causes GC to run and mess up the results

	// Time compilation with no cached data
	const uncachedCompileStart = Date.now();
	for (let i = 0; i < count; i++) {
		const isolate = new ivm.Isolate;
		const script = isolate.compileScriptSync(src);
	}
	const uncachedCompileEnd = Date.now();
	const uncachedCompileDuration = uncachedCompileEnd - uncachedCompileStart;

	// Time compilation with cached data
	const cachedCompileStart = Date.now();
	for (let i = 0; i < count; i++) {
		const isolate = new ivm.Isolate;
		const script = isolate.compileScriptSync(src, { cachedData });
		assert.ok(!script.cachedDataRejected);
	}
	const cachedCompileEnd = Date.now();
	const cachedCompileDuration = cachedCompileEnd - cachedCompileStart;

	if (cachedCompileDuration > uncachedCompileDuration * 0.7) {
		console.log('cached data is suspiciously slow');
	}
}

// Check module cached data
{
	const cachedData = (() => {
		const isolate = new ivm.Isolate;
		const module = isolate.compileModuleSync(src, { produceCachedData: true });
		assert.ok(module.cachedData);
		assert.strictEqual(module.cachedDataRejected, undefined);
		return module.cachedData;
	})();
	const isolate = new ivm.Isolate;
	const module = isolate.compileModuleSync(src, { cachedData });
	assert.strictEqual(module.cachedData, undefined);
	assert.ok(!module.cachedDataRejected);
}

console.log('pass');
