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
