import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as ivm from "@isolated-vm/experimental";
import { makeNullLinker } from "../utility/linker.js";
import { expectComplete, expectThrow } from "./fixtures.js";

await test("module linker", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	const left = expectComplete(await agent.compileModule('import { right } from "right"; globalThis.right = right;'));
	const right = expectComplete(await agent.compileModule('export function right() { return "hello"; }'));
	await left.link(realm, specifier => {
		assert.equal(specifier, "right");
		return right;
	});
	await left.evaluate(realm);
	const script = expectComplete(await agent.compileScript("right();"));
	const result = expectComplete(await script.run(realm));
	assert.equal(result, "hello");
});

await test("nested compilation in module linker", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	const left = expectComplete(await agent.compileModule('import "right";'));
	await left.link(realm, async () => expectComplete(await agent.compileModule("export {};")));
});

await test("synthetic module capability", async () => {
	await using agent = await ivm.Agent.create();
	const capabilityName = "isolated-vm:///capability";
	let didInvoke = false;
	const realm = await agent.createRealm();
	const capability = await realm.createCapability(
		() => ({
			default: (value1: unknown, value2: unknown) => {
				didInvoke = true;
				assert.equal(value1, "hello");
				assert.equal(value2, "world");
			},
		}),
		{ origin: capabilityName });
	const left = expectComplete(await agent.compileModule(`
		import capability from ${JSON.stringify(capabilityName)};
		capability("hello", "world");
	`));
	const cache = new Map([
		[ capabilityName, capability ],
	]);
	await left.link(realm, specifier => cache.get(specifier)!);
	await left.evaluate(realm);
	assert.ok(didInvoke);
});

// await test("synthetic module with circular reference", async () => {
// 	await using agent = await ivm.Agent.create();
// 	const capabilityName = "isolated-vm:///capability";
// 	const realm = await agent.createRealm();
// 	const capability = await realm.createCapability(
// 		() => ({
// 			default: (value1: unknown) => {
// 				// @ts-expect-error
// 				assert.strictEqual(value1.date1, value1.date2);
// 				// @ts-expect-error
// 				assert.strictEqual(value1.object, value1);
// 			},
// 		}),
// 		{ origin: capabilityName });
// 	const left = expectComplete(await agent.compileModule(`
// 		import capability from ${JSON.stringify(capabilityName)};
// 		const date = new Date();
// 		const object = {
// 			record: {},
// 			date1: date,
// 			date2: date,
// 		};
// 		object.object = object;
// 		capability(object);
// 	`));
// 	const cache = new Map([
// 		[ capabilityName, capability ],
// 	]);
// 	await left.link(realm, specifier => cache.get(specifier)!);
// 	await left.evaluate(realm);
// });

// TODO: This terminates the process. It should probably raise an async error, because there is no
// promise.
// await test("throw from synthetic module", async () => {
// 	await using agent = await ivm.Agent.create();
// 	const capabilityName = "isolated-vm:///capability";
// 	const realm = await agent.createRealm();
// 	const capability = await realm.createCapability(
// 		() => ({
// 			default: () => {
// 				throw new Error("capability error");
// 			},
// 		}),
// 		{ origin: capabilityName });
// 	const left = expectComplete(await agent.compileModule(`
// 		import capability from ${JSON.stringify(capabilityName)};
// 		capability();
// 	`));
// 	const cache = new Map([
// 		[ capabilityName, capability ],
// 	]);
// 	await left.link(realm, specifier => cache.get(specifier)!);
// 	await left.evaluate(realm);
// });

await test("throw from linker", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	const left = expectComplete(await agent.compileModule('import "right";'));
	await assert.rejects(left.link(realm, () => {
		throw new Error("linker error");
	}));
});

await test("throw from evaluation", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	const module = expectComplete(await agent.compileModule('throw new Error("wow");', { origin: { name: "test:module" } }));
	await module.link(realm, makeNullLinker());
	const error = expectThrow(await module.evaluate(realm)) as Error;
	assert.equal(error.message, "wow");
	assert.ok(error.stack?.includes("test:module"));
});

await test("missing imports", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	const left = expectComplete(await agent.compileModule('import { nothing } from "right";'));
	await assert.rejects(left.link(realm, async () =>
		expectComplete(await agent.compileModule("export {};"))));
});
