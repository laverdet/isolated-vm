import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as ivm from "@isolated-vm/experimental";
import { makeCachedLoader, makeFileSystemCompilationLoader } from "../utility/linker.js";
import { expectComplete } from "./fixtures.js";

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

await test("rethrow from linker", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	const left = expectComplete(await agent.compileModule('import "right";'));
	await assert.rejects(left.link(realm, () => {
		throw new Error("linker error");
	}));
});

await test("missing imports", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	const left = expectComplete(await agent.compileModule('import { nothing } from "right";'));
	await assert.rejects(left.link(realm, async () =>
		expectComplete(await agent.compileModule("export {};"))));
});
