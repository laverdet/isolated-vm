import type { Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, expect } from "@isolated-vm/experimental";
import { expectComplete, unsafeEvalAsStringInRealm } from "@isolated-vm/experimental/test/fixtures";

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
		// @ts-expect-error
		globalThis.prox = new Proxy({}, {
			// @ts-expect-error
			get() { globalThis.failed = true; },
			// @ts-expect-error
			set() { globalThis.failed = true; },
		});
	});
	const global = await realm.acquireGlobalObject();
	const prox = expect(await global.get("prox"));
	await prox.get("name");
	assert.equal(await expect(await global.get("failed")).copy(), undefined);
	expect(await prox.set("name", undefined));
	assert.equal(await expect(await global.get("failed")).copy(), undefined);

	// sanity check
	await unsafeEvalAsStringInRealm(agent, realm, () => {
		// @ts-expect-error
		globalThis.failed = true;
	});
	assert.equal(await expect(await global.get("failed")).copy(), true);
});
