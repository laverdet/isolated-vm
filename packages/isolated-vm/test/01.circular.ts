import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as ivm from "isolated-vm";
import { unsafeEvalAsStringInRealm } from "./fixtures.js";

await test("transfer circular object references", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	const result = await unsafeEvalAsStringInRealm(agent, realm, () => {
		const date = new Date();
		const object: Record<string, any> = {
			record: {},
			date1: date,
			date2: date,
		};
		object.object = object;
		return object;
	});
	assert.strictEqual(result.date1, result.date2);
	assert.strictEqual(result.object, result);
});
