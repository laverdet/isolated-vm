import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as ivm from "isolated-vm";
import { unsafeEvalAsString } from "./fixtures.js";

const makeAgentRandom =	async (randomSeed?: number) => {
	await using agent = await ivm.Agent.create({ randomSeed });
	return await unsafeEvalAsString(agent, () => Math.random());
};

await test("random", async () => {
	const result1 = await makeAgentRandom();
	const result2 = await makeAgentRandom();
	assert.notEqual(result1, result2);
});

await test("seeded random", async () => {
	const result1 = await makeAgentRandom(123);
	const result2 = await makeAgentRandom(123);
	assert.equal(result1, result2);
});

await test("different seeds", async () => {
	const result1 = await makeAgentRandom(123);
	const result2 = await makeAgentRandom(456);
	assert.notEqual(result1, result2);
});
