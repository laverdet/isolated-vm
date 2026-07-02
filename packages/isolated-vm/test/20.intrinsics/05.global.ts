import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, expect } from "@isolated-vm/experimental";
import { unsafeEvalAsStringInRealm } from "@isolated-vm/experimental/test/fixtures";

await test("realm global reference", async () => {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	await unsafeEvalAsStringInRealm(agent, realm, () => {
		// @ts-expect-error
		globalThis.test = 123;
	});
	const global = await realm.acquireGlobalObject();
	const property = expect(await global.get("test"));
	const value = await property.copy();
	assert.equal(value, 123);
});
