import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as ivm from "isolated-vm";
import { unsafeEvalAsStringInRealm } from "./fixtures.js";

async function check(agent: ivm.Agent, realm: ivm.Realm, fn: () => unknown) {
	const result = await unsafeEvalAsStringInRealm(agent, realm, fn);
	assert.deepStrictEqual(result, fn());
}

await test("transfer types", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();

	await check(agent, realm, () => "hello world");
	await check(agent, realm, () => "\u{10ffff}");
	await check(agent, realm, () => 0);
	await check(agent, realm, () => Number.MAX_SAFE_INTEGER);
	await check(agent, realm, () => Number.MAX_SAFE_INTEGER + 1);
	await check(agent, realm, () => Number.MIN_SAFE_INTEGER);
	await check(agent, realm, () => Number.MIN_SAFE_INTEGER - 1);
	await check(agent, realm, () => Math.PI);
});
