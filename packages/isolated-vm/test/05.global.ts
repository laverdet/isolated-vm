import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as ivm from "@isolated-vm/experimental";
import { unsafeEvalAsStringInRealm } from "./fixtures.js";

await test("realm global reference", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	await unsafeEvalAsStringInRealm(agent, realm, () => {
		// @ts-expect-error
		globalThis.test = 123;
	});
	const global = await realm.acquireGlobalObject();
	const property = await global.get("test");
	const value = await property.copy();
	assert.equal(value, 123);
});
