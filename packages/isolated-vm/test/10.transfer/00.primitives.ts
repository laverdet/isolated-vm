import type { TestAgent } from "@isolated-vm/experimental/test/fixtures";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

async function check(vm: TestAgent, fn: () => unknown) {
	const result = await vm.unsafeScript(fn);
	assert.deepStrictEqual(result, fn());
}

await test("primitives", async () => {
	await using vm = await makeTestAgent();

	// null, undefined, bool
	await check(vm, () => null);
	await check(vm, () => undefined);
	await check(vm, () => true);
	await check(vm, () => false);

	// boolean
	await check(vm, () => true);
	await check(vm, () => false);

	// string
	await check(vm, () => "hello world");
	await check(vm, () => "\u{10ffff}");

	// number
	await check(vm, () => 0);
	await check(vm, () => Number.MAX_SAFE_INTEGER);
	await check(vm, () => Number.MAX_SAFE_INTEGER + 1);
	await check(vm, () => Number.MIN_SAFE_INTEGER);
	await check(vm, () => Number.MIN_SAFE_INTEGER - 1);
	await check(vm, () => Math.PI);

	// bigint
	await check(vm, () => 0n);
	await check(vm, () => 9_223_372_036_854_775_807n); // int64 max
	await check(vm, () => 9_223_372_036_854_775_808n); // int64 max + 1
	await check(vm, () => -9_223_372_036_854_775_808n); // int64 min
	await check(vm, () => -9_223_372_036_854_775_809n); // int64 min - 1
	await check(vm, () => 18_446_744_073_709_551_615n); // uint64 max
	await check(vm, () => 18_446_744_073_709_551_616n); // uint64 max + 1
	await check(vm, () => 502_360_950_888_298_027_355_518_043_327_124_471_675_959_709_617_316_205_938_938_264_006_428_765_255_489n); // 256 bit number
	await check(vm, () => -502_360_950_888_298_027_355_518_043_327_124_471_675_959_709_617_316_205_938_938_264_006_428_765_255_490n); // 256 bit negative
});
