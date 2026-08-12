const ivm = require('isolated-vm');
const { strictEqual } = require('assert');

(async function() {
	const isolate = new ivm.Isolate();
	const context = await isolate.createContext();

	// A module which finishes synchronously settles right away, and the value is
	// undefined: the evaluation promise fulfills with no value.
	{
		const module = await isolate.compileModule('globalThis.ran = true;');
		await module.instantiate(context, () => {});
		strictEqual(await module.evaluate({ promise: true }), undefined);
		strictEqual(await context.global.get('ran'), true);
	}

	// Without the option `evaluate` still resolves when the module yields.
	{
		const module = await isolate.compileModule(
			'await new Promise(resolve => globalThis.wake1 = resolve); globalThis.finished1 = true;');
		await module.instantiate(context, () => {});
		strictEqual(await module.evaluate(), undefined);
		strictEqual(await context.global.get('finished1'), undefined);
	}

	// With the option `evaluate` resolves once the module has finished.
	{
		const module = await isolate.compileModule(
			'await new Promise(resolve => globalThis.wake2 = resolve); globalThis.finished2 = true;');
		await module.instantiate(context, () => {});
		let woke = false;
		const settled = module.evaluate({ promise: true }).then(() => strictEqual(woke, true));
		// Without this catch an assertion failure surfaces as an unhandled rejection
		// before the await below reads it.
		settled.catch(() => {});
		await new Promise(resolve => setTimeout(resolve, 100));
		woke = true;
		await context.eval('wake2()');
		await settled;
		strictEqual(await context.global.get('finished2'), true);
	}

	// A throw after the first yield rejects `evaluate` with the module's own error.
	{
		const module = await isolate.compileModule(
			'await new Promise(resolve => globalThis.wake3 = resolve); throw new Error("late failure");');
		await module.instantiate(context, () => {});
		const settled = module.evaluate({ promise: true }).then(
			() => { throw new Error('Promise did not reject') },
			error => strictEqual(error.message, 'late failure'));
		await new Promise(resolve => setTimeout(resolve, 100));
		await context.eval('wake3()');
		await settled;
	}

	// `evaluateSync` returns a promise which settles the same way.
	{
		const module = await isolate.compileModule(
			'await new Promise(resolve => globalThis.wake4 = resolve); globalThis.finished4 = true;');
		await module.instantiate(context, () => {});
		let woke = false;
		const settled = module.evaluateSync({ promise: true }).then(() => strictEqual(woke, true));
		settled.catch(() => {});
		await new Promise(resolve => setTimeout(resolve, 100));
		woke = true;
		await context.eval('wake4()');
		await settled;
		strictEqual(await context.global.get('finished4'), true);
	}

	// A throw before the first yield rejects the returned promise; the call itself
	// throws nothing.
	{
		const module = await isolate.compileModule('throw new Error("early failure");');
		await module.instantiate(context, () => {});
		await module.evaluateSync({ promise: true }).then(
			() => { throw new Error('Promise did not reject') },
			error => strictEqual(error.message, 'early failure'));
	}

	// The timeout still bounds the run up to the first yield.
	{
		const module = await isolate.compileModule('let i = 0; while (++i); i;');
		await module.instantiate(context, () => {});
		await module.evaluate({ timeout: 50, promise: true }).then(
			() => { throw new Error('Promise did not reject') },
			error => strictEqual(error.message, 'Script execution timed out.'));
	}

	// Repeat calls answer with the first invocation's outcome, while the module
	// is still running and after it has failed.
	{
		const module = await isolate.compileModule(
			'await new Promise(resolve => globalThis.wake5 = resolve); throw new Error("repeated failure");');
		await module.instantiate(context, () => {});
		const outcomes = [
			module.evaluate({ promise: true }),
			module.evaluate({ promise: true }),
		].map(settled => settled.then(
			() => { throw new Error('Promise did not reject') },
			error => strictEqual(error.message, 'repeated failure')));
		await context.eval('wake5()');
		await Promise.all(outcomes);
		await module.evaluate({ promise: true }).then(
			() => { throw new Error('Promise did not reject') },
			error => strictEqual(error.message, 'repeated failure'));
	}

	// Disposing the isolate rejects a pending settlement.
	{
		const other = new ivm.Isolate();
		const otherContext = await other.createContext();
		const module = await other.compileModule('await new Promise(() => {});');
		await module.instantiate(otherContext, () => {});
		const settled = module.evaluate({ promise: true }).then(
			() => { throw new Error('Promise did not reject') },
			error => strictEqual(error.message, 'Promise was abandoned'));
		await new Promise(resolve => setTimeout(resolve, 100));
		other.dispose();
		await settled;
	}

	console.log('pass');
})().catch(console.error);
