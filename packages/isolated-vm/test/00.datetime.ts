import * as assert from "node:assert/strict";
import { test } from "node:test";
import { setTimeout } from "node:timers/promises";
import { unsafeEvalAsString, unsafeEvalAsStringInRealm, withAgent } from "./fixtures.js";

const y2k = new Date("2000-01-01T00:00:00Z");
// Figure out how many loops to waste ~25ms on this build. Extra slop factor is added afterwards.
const speedFactorMs = 25;
const speedFactor = 1.25 * await withAgent(null, agent =>
	unsafeEvalAsString(
		agent, speedFactorMs => {
			let factor = 2 ** 20;
			while (true) {
				const now = Date.now();
				for (let ii = factor; ii > 0; --ii);
				const timeTaken = Math.max(1, Date.now() - now);
				factor = Math.floor(factor * speedFactorMs / timeTaken);
				if (timeTaken >= speedFactorMs) {
					return Math.floor(factor);
				}
			}
		}, speedFactorMs),
);

await test("deterministic clock", async () => {
	await withAgent({
		clock: { type: "deterministic", epoch: y2k, interval: 1 },
	}, async agent => {
		const realm = await agent.createRealm();
		let previousTime = +await unsafeEvalAsStringInRealm(agent, realm, () => new Date());
		assert.equal(previousTime, +y2k);
		for (let ii = 0; ii < 25; ++ii) {
			const result2 = await unsafeEvalAsStringInRealm(agent, realm, () => new Date());
			assert.equal(+result2, ++previousTime);
		}
	});
});

await test("microtask clock", async () => {
	await withAgent({
		clock: { type: "microtask", epoch: y2k },
	}, async agent => {
		const realm = await agent.createRealm();
		const result1 = await unsafeEvalAsStringInRealm(agent, realm, () => new Date());
		assert.ok(+result1 >= +y2k);
		assert.ok(+result1 - +y2k < 1000);
		await setTimeout(10);
		const result2 = await unsafeEvalAsStringInRealm(agent, realm, () => new Date());
		assert.ok(+result2 >= +result1 + 10);
		const result3 = await unsafeEvalAsStringInRealm(agent, realm, speedFactor => {
			const dd = Date.now();
			for (let ii = 0; ii < speedFactor; ++ii);
			return Date.now() - dd;
		}, speedFactor);
		assert.equal(result3, 0);
	});
});

await test("realtime clock", async () => {
	await withAgent({
		clock: { type: "realtime", epoch: y2k },
	}, async agent => {
		const realm = await agent.createRealm();
		const result1 = await unsafeEvalAsStringInRealm(agent, realm, () => new Date());
		assert.ok(+result1 >= +y2k);
		assert.ok(+result1 <= +y2k + 1000);
		const result2 = await unsafeEvalAsStringInRealm(agent, realm, (speedFactor: number) => {
			for (let ii = 0; ii < speedFactor; ++ii);
			return new Date();
		}, speedFactor);
		assert.ok(+result2 - +result1 > speedFactorMs);
	});
});

await test("system clock", async () => {
	await withAgent(null, async agent => {
		const realm = await agent.createRealm();
		for (let ii = 0; ii < 5; ++ii) {
			const then = new Date();
			const result = await unsafeEvalAsStringInRealm(agent, realm, () => new Date());
			assert.ok(+result >= +then);
			assert.ok(+result <= Date.now());
			await setTimeout(1);
		}
	});
});
