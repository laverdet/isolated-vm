import type { Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { inspect } from "node:util";
import { Agent, Capability, expect } from "@isolated-vm/experimental";
import { expectComplete, unsafeEvalAsStringInRealm } from "@isolated-vm/experimental/test/fixtures";

const illegalAccess = new Error("illegal access");

await test("function reference invoke", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	await unsafeEvalAsStringInRealm(agent, realm, () => {
		// @ts-expect-error
		globalThis.fn = () => "wow";
	});
	const global = await realm.acquireGlobalObject();
	const property = (await global.get("fn")) as Reference<() => unknown>;
	const value = expectComplete(await property.invoke([]));
	assert.equal(value, "wow");
});

await test("invoke function cross-param reference", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	await unsafeEvalAsStringInRealm(agent, realm, () => {
		// @ts-expect-error
		globalThis.fn = (...values: unknown[]) => values;
	});
	const global = await realm.acquireGlobalObject();
	const fn = await global.get("fn");
	const object = {};
	// @ts-expect-error
	const [ left, right ] = expectComplete(await fn.invoke([ object, object ]));
	assert.strictEqual(left, right);
});

await test("invoke function reference with circular object", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	await unsafeEvalAsStringInRealm(agent, realm, () => {
		// @ts-expect-error
		globalThis.fn = (value: unknown) => value;
	});
	const global = await realm.acquireGlobalObject();
	const fn = (await global.get("fn")) as Reference<(value: unknown) => unknown>;
	const date = new Date();
	const object: Record<string, any> = {
		record: {},
		date1: date,
		date2: date,
	};
	object.object = object;
	const result = expectComplete(await fn.invoke([ object ]));
	// @ts-expect-error
	assert.strictEqual(result.date1, result.date2);
	// @ts-expect-error
	assert.strictEqual(result.object, result);
});

await test("get & set should not invoke proxies", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	await unsafeEvalAsStringInRealm(agent, realm, () => {
		const proxy = new Proxy({}, {
			get() { throw new Error("interceptor invoked"); },
			set() { throw new Error("interceptor invoked"); },
		});
		// @ts-expect-error
		globalThis.proxy = proxy;
	});
	const global = await realm.acquireGlobalObject();
	const proxy = expect(await global.get("proxy"));
	expect(await proxy.get("name"));
	expect(await proxy.set("name", undefined));
});

await test("copy should not invoke interceptors", async () => {
	await using agent = await Agent.create();
	const realm = await agent.createRealm();
	await unsafeEvalAsStringInRealm(agent, realm, () => {
		const object =
			Object.defineProperty({}, "name", {
				enumerable: true,
				get() { throw new Error("interceptor invoked"); },
			});
		// @ts-expect-error
		globalThis.object = object;
	});
	const global = await realm?.acquireGlobalObject();
	const object = await global?.get("object");
	await assert.rejects(async () => object?.copy(), illegalAccess);
});

await test("invoke should not invoke interceptors", async () => {
	await using agent = await Agent.create();
	const realm = await agent.createRealm();
	await unsafeEvalAsStringInRealm(agent, realm, () => {
		// @ts-expect-error
		globalThis.fn = () => {};
		// eslint-disable-next-line no-extend-native
		Object.defineProperty(Object.prototype, "name", {
			set() { throw new Error("interceptor invoked"); },
		});
	});
	const global = await realm?.acquireGlobalObject();
	const fn = await global?.get("fn") as Reference<(...args: unknown[]) => unknown> | null;
	await assert.rejects(
		async () => expectComplete(await fn?.invoke([ { name: "value" } ])),
		illegalAccess,
	);
});

await test("copy settles immediately when the isolate is idle, and waits when it is busy", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	// `started` resolves once the busy loop is running, and therefore holding the isolate lock
	const { promise: started, resolve: onStarted } = Promise.withResolvers<null>();
	const signal = expect(await realm.createCapability({ default: Capability.void(() => onStarted(null)) }, { origin: "signal" }));
	const module = expectComplete(await agent.compileModule(`
		import signal from "signal";
		globalThis.value = { nested: [ 1, "two", true ] };
		globalThis.busy = () => { signal(); const end = Date.now() + 200; while (Date.now() < end); };
	`));
	await module.link(realm, () => signal);
	expectComplete(await module.evaluate(realm));
	const global = await realm.acquireGlobalObject();
	const value = expect(await global.get("value"));

	// Idle: only works sometimes. If there's a crash the test will fail intermittently.
	// eslint-disable-next-line @typescript-eslint/await-thenable
	await undefined;
	const idle = value.copy();
	assert.deepEqual(await idle, { nested: [ 1, "two", true ] });

	// Busy: the copy falls back to the scheduler and completes once the isolate is free
	const busy = expect(await global.get("busy")) as Reference<() => void>;
	const running = busy.invoke([]);
	await started;
	const deferred = value.copy();
	assert.match(inspect(deferred), /pending/);
	assert.deepEqual(await deferred, { nested: [ 1, "two", true ] });
	expectComplete(await running);
});
