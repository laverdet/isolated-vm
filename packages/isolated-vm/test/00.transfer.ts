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

	// string
	await check(agent, realm, () => "hello world");
	await check(agent, realm, () => "\u{10ffff}");

	// number
	await check(agent, realm, () => 0);
	await check(agent, realm, () => Number.MAX_SAFE_INTEGER);
	await check(agent, realm, () => Number.MAX_SAFE_INTEGER + 1);
	await check(agent, realm, () => Number.MIN_SAFE_INTEGER);
	await check(agent, realm, () => Number.MIN_SAFE_INTEGER - 1);
	await check(agent, realm, () => Math.PI);

	// bigint
	await check(agent, realm, () => 0n);
	await check(agent, realm, () => 9_223_372_036_854_775_807n); // int64 max
	await check(agent, realm, () => 9_223_372_036_854_775_808n); // int64 max + 1
	await check(agent, realm, () => -9_223_372_036_854_775_808n); // int64 min
	await check(agent, realm, () => -9_223_372_036_854_775_809n); // int64 min - 1
	await check(agent, realm, () => 18_446_744_073_709_551_615n); // uint64 max
	await check(agent, realm, () => 18_446_744_073_709_551_616n); // uint64 max + 1
	await check(agent, realm, () => 502_360_950_888_298_027_355_518_043_327_124_471_675_959_709_617_316_205_938_938_264_006_428_765_255_489n); // 256 bit number
	await check(agent, realm, () => -502_360_950_888_298_027_355_518_043_327_124_471_675_959_709_617_316_205_938_938_264_006_428_765_255_490n); // 256 bit negative
});
