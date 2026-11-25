import * as assert from "node:assert/strict";
import { test } from "node:test";
import { makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

await test("memory limit", async () => {
	await using vm = await makeTestAgent({ memoryLimitBytes: 128 << 20 });
	assert.strictEqual(await vm.unsafeScript(() => {
		const x: string[] = [];
		for (;;) {
			let a = "a";
			for (; a.length < 2 ** 28;) a += a;
			x.push(a);
		}
	}), null);
});

await test("array spread", async () => {
	await using vm = await makeTestAgent({ memoryLimitBytes: 128 << 20 });
	assert.strictEqual(await vm.unsafeScript(() => [ ...".".repeat(5e7) ].length), null);
});
