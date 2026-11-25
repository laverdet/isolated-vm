import * as assert from "node:assert/strict";
import { once } from "node:events";
import { test } from "node:test";
import { Worker } from "node:worker_threads";
import { isBun } from "@isolated-vm/experimental/test/fixtures";

// nb: This test should be run individually:
//   node --test test/90.isolation/30.undispose.ts
const processIsolation = { skip: isBun && "bun runs every test file in one process" };

const specifier = import.meta.resolve("@isolated-vm/experimental");

// Also runs in the worker, as a string
async function runScript(specifier: string) {
	const { Agent, expect, expectComplete } = await import(specifier) as typeof import("@isolated-vm/experimental");
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const script = expectComplete(await agent.compileScript("1 + 1"));
	return expectComplete(await script.run(realm));
}

await test("undispose", processIsolation, async () => {
	const worker = new Worker(`
		const { parentPort } = require("node:worker_threads");
		(${String(runScript)})(${JSON.stringify(specifier)}).then(result => parentPort.postMessage(result));
	`, { eval: true });
	const exited = once(worker, "exit") as Promise<[ number ]>;
	const [ result ] = await once(worker, "message") as [ unknown ];
	assert.strictEqual(result, 2);
	const [ exitCode ] = await exited;
	assert.strictEqual(exitCode, 0);
	assert.strictEqual(await runScript(specifier), 2);
});
