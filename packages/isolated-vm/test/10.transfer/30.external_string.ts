import type { Reference } from "@isolated-vm/experimental";
import type { TestAgent } from "@isolated-vm/experimental/test/fixtures";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { expectComplete, makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

// The agent is created with a heap limit much lower than the externalized string. So this would
// fail if they weren't externalized.
const memoryLimitBytes = 32 << 20;
const stringBytes = 96 << 20;
const allocate = () => {
	const array: { ii: number }[] = [];
	for (let ii = 0; ii < 5e5; ++ii) array.push({ ii });
	return array.length;
};

type Retain = (string: string, length: number, code: number) => void;

/** Returns a reference to a function which keeps its argument alive in the realm, after checking it */
async function makeRetain(vm: TestAgent) {
	const make = await vm.defineFunction(() => {
		const retained: string[] = [];
		return new rt.Reference((string: string, length: number, code: number) => {
			retained.push(string);
			assert.equal(string.length, length);
			assert.equal(string.charCodeAt(0), code);
			assert.equal(string.charCodeAt(length - 1), code);
		});
	});
	return expectComplete(await make.invoke([])) as unknown as Reference<Retain>;
}

await test("large latin1 string is externalized", async () => {
	await using vm = await makeTestAgent({ memoryLimitBytes });
	const fn = await makeRetain(vm);
	const string = "a".repeat(stringBytes);
	expectComplete(await fn.invoke([ string, string.length, 0x61 ]));
	assert.equal(await vm.unsafeScript(allocate), 5e5);
});

await test("large utf16 string is externalized", async () => {
	await using vm = await makeTestAgent({ memoryLimitBytes });
	const fn = await makeRetain(vm);
	const string = "中".repeat(stringBytes / 2);
	expectComplete(await fn.invoke([ string, string.length, 0x4e2d ]));
	assert.equal(await vm.unsafeScript(allocate), 5e5);
});
