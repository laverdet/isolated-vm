import * as assert from "node:assert/strict";
import { test } from "node:test";
import * as ivm from "isolated-vm";

await test("module linker", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	const left = await agent.compileModule('import { right } from "right"; globalThis.right = right;');
	const right = await agent.compileModule('export function right() { return "hello"; }');
	await left.link(realm, specifier => {
		assert.equal(specifier, "right");
		return right;
	});
	await left.evaluate(realm);
	const script = await agent.compileScript("right();");
	const result = await script.run(realm);
	assert.equal(result, "hello");
});

await test("nested compilation in module linker", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();
	const left = await agent.compileModule('import "right";');
	await left.link(realm, () => agent.compileModule("export {};"));
});

await test("synthetic module capability", async () => {
	await using agent = await ivm.Agent.create();
	const capabilityName = "isolated-vm:///capability";
	let didInvoke = false;
	const realm = await agent.createRealm();
	const capability = await agent.createCapability(
		(value1: unknown, value2: unknown) => {
			didInvoke = true;
			assert.equal(value1, "hello");
			assert.equal(value2, "world");
		},
		{ origin: { name: capabilityName } },
	);
	const left = await agent.compileModule(`
		import capability from ${JSON.stringify(capabilityName)};
		capability("hello", "world");
	`);
	const cache = new Map([
		[ capabilityName, capability ],
	]);
	await left.link(realm, specifier => cache.get(specifier)!);
	await left.evaluate(realm);
	assert.ok(didInvoke);
});
