import * as assert from "node:assert/strict";
import { test } from "node:test";
import { setTimeout } from "node:timers/promises";
import * as ivm from "isolated-vm";
import { unsafeEvalAsString, unsafeEvalAsStringInRealm } from "./fixtures.js";

const y2k = new Date("2000-01-01T00:00:00Z");
// Figure out how many loops to waste ~25ms on this build. Extra slop factor is added afterwards.
const speedFactorMs = 25;
const speedFactor = await async function() {
	await using agent = await ivm.Agent.create();
	return await unsafeEvalAsString(
		agent, speedFactorMs =>
			Math.min(...Array(3).fill(undefined).map(() => {
				let factor = 2 ** 20;
				const slopFactor = 1.2;
				while (true) {
					const now = Date.now();
					for (let ii = factor; ii > 0; --ii);
					const timeTaken = Math.max(1, Date.now() - now);
					factor = Math.floor(factor * speedFactorMs / timeTaken);
					if (timeTaken >= speedFactorMs) {
						return Math.floor(factor * slopFactor);
					}
				}
			})), speedFactorMs);
}();

await test("deterministic clock", async () => {
	await using agent = await ivm.Agent.create({
		clock: { type: "deterministic", epoch: y2k, interval: 1 },
	});
	const realm = await agent.createRealm();
	let previousTime = +await unsafeEvalAsStringInRealm(agent, realm, () => new Date());
	assert.equal(previousTime, +y2k);
	for (let ii = 0; ii < 25; ++ii) {
		const result2 = await unsafeEvalAsStringInRealm(agent, realm, () => new Date());
		assert.equal(+result2, ++previousTime);
	}
});

await test("microtask clock", async () => {
	await using agent = await ivm.Agent.create({
		clock: { type: "microtask", epoch: y2k },
	});
	const realm = await agent.createRealm();
	const result1 = await unsafeEvalAsStringInRealm(agent, realm, () => new Date());
	assert.ok(+result1 >= +y2k);
	assert.ok(+result1 - +y2k < 1000);
	await setTimeout(10);
	const result2 = await unsafeEvalAsStringInRealm(agent, realm, () => new Date());
	assert.ok(+result2 >= +result1 + 10);
	const result3 = await unsafeEvalAsStringInRealm(agent, realm, speedFactor => {
		const dd = Date.now();
		for (let ii = speedFactor; ii > 0; --ii);
		return Date.now() - dd;
	}, speedFactor);
	assert.equal(result3, 0);
});

await test("realtime clock", async () => {
	await using agent = await ivm.Agent.create({
		clock: { type: "realtime", epoch: y2k },
	});
	const realm = await agent.createRealm();
	const [ result1 ] = await unsafeEvalAsStringInRealm(agent, realm, (speedFactor: number) => {
		const start = new Date();
		for (let ii = speedFactor; ii > 0; --ii);
		const end = new Date();
		return [ start, end ];
	}, speedFactor);
	assert.ok(+result1 >= +y2k);
	assert.ok(+result1 <= +y2k + 1000);
	// assert.ok(+result2 - +result1 >= speedFactorMs);
});

await test("system clock", async () => {
	// `std::system_clock` on Windows returns jittering results across different CPU cores.
	const threshold = process.platform === "win32" ? 10 : 0;
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	for (let ii = 0; ii < 5; ++ii) {
		const then = new Date();
		const result = await unsafeEvalAsStringInRealm(agent, realm, () => new Date());
		assert.ok(+result >= +then - threshold);
		assert.ok(+result <= Date.now() + threshold);
		await setTimeout(1);
	}
});
