import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as ivm from "@isolated-vm/experimental";
import { expectComplete, unsafeEvalAsStringInRealm } from "./fixtures.js";

await test("function reference invoke", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	await unsafeEvalAsStringInRealm(agent, realm, () => {
		// @ts-expect-error
		globalThis.fn = () => "wow";
	});
	const global = await realm.acquireGlobalObject();
	const property = await global.get("fn");
	const value = expectComplete(await property.invoke([]));
	assert.equal(value, "wow");
});

await test("invoke function reference with circular object", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	await unsafeEvalAsStringInRealm(agent, realm, () => {
		// @ts-expect-error
		globalThis.fn = (value: unknown) => value;
	});
	const global = await realm.acquireGlobalObject();
	const fn = await global.get("fn");
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
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
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
	const prox = await global.get("prox");
	await prox.get("name");
	assert.equal(await (await global.get("failed")).copy(), undefined);
	await prox.set("name", undefined);
	assert.equal(await (await global.get("failed")).copy(), undefined);

	// sanity check
	await unsafeEvalAsStringInRealm(agent, realm, () => {
		// @ts-expect-error
		globalThis.failed = true;
	});
	assert.equal(await (await global.get("failed")).copy(), true);
});
