import type { Reference } from "@isolated-vm/experimental";
import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, expect } from "@isolated-vm/experimental";
import { expectComplete, unsafeEvalAsStringInRealm } from "@isolated-vm/experimental/test/fixtures";

// The agent is created with a heap limit much lower than the externalized string. So this would
// fail if they weren't externalized.
const memoryLimitBytes = 32 << 20;
const stringBytes = 96 << 20;
const allocateSourceText = "const array = []; for (let ii = 0; ii < 5e5; ++ii) array.push({ ii }); array.length";

type Retain = (string: string) => [ number, number, string ];

async function makeRetain(agent: Agent) {
	const realm = expect(await agent.createRealm());
	await unsafeEvalAsStringInRealm(agent, realm, () => {
		// @ts-expect-error
		globalThis.fn = (string: string) => {
			// @ts-expect-error
			globalThis.retained = string;
			return [ string.length, string.charCodeAt(0), string.at(-1) ];
		};
	});
	const global = await realm.acquireGlobalObject();
	const fn = expect(await global.get("fn")) as Reference<Retain>;
	return { realm, fn };
}

await test("large latin1 string is externalized", async () => {
	await using agent = await Agent.create({ memoryLimitBytes });
	const { realm, fn } = await makeRetain(agent);
	const string = "a".repeat(stringBytes);
	const result = expectComplete(await fn.invoke([ string ]));
	assert.deepStrictEqual(result, [ string.length, 97, "a" ]);
	const script = expectComplete(await agent.compileScript(allocateSourceText));
	assert.equal(expectComplete(await script.run(realm)), 5e5);
});

await test("large utf16 string is externalized", async () => {
	await using agent = await Agent.create({ memoryLimitBytes });
	const { realm, fn } = await makeRetain(agent);
	const string = "中".repeat(stringBytes / 2);
	const result = expectComplete(await fn.invoke([ string ]));
	assert.deepStrictEqual(result, [ string.length, 0x4e2d, "中" ]);
	const script = expectComplete(await agent.compileScript(allocateSourceText));
	assert.equal(expectComplete(await script.run(realm)), 5e5);
});
