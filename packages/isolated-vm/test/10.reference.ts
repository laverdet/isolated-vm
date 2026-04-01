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
