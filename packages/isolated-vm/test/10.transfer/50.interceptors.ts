import * as assert from "node:assert/strict";
import { describe, test } from "node:test";
import { Agent } from "@isolated-vm/experimental";
import { unsafeEvalAsStringInRealm } from "@isolated-vm/experimental/test/fixtures";

const supportsExpectedFailure =
	// @ts-expect-error
	process.isBun === undefined && process.versions.deno === undefined;

if (supportsExpectedFailure) {
	// None of these work!
	await test("named proxy", { expectFailure: "interceptor invoked" }, async () => {
		await using agent = await Agent.create();
		const realm = await agent.createRealm();
		const result = await unsafeEvalAsStringInRealm(agent, realm, () => {
			const proxy = new Proxy({ name: "value" }, {
				get() { throw new Error("interceptor invoked"); },
			});
			return proxy;
		});
		assert.deepStrictEqual(result, { name: undefined });
	});

	await test("indexed proxy", { expectFailure: "interceptor invoked" }, async () => {
		await using agent = await Agent.create();
		const realm = await agent.createRealm();
		const result = await unsafeEvalAsStringInRealm(agent, realm, () => {
			const proxy = new Proxy([ 0, 1 ], {
				get() { throw new Error("interceptor invoked"); },
			});
			return proxy;
		});
		assert.deepStrictEqual(result, [ undefined, undefined ]);
	});

	await describe("named getter", { expectFailure: "interceptor invoked" }, async () => {
		await using agent = await Agent.create();
		const realm = await agent.createRealm();
		const result = await unsafeEvalAsStringInRealm(agent, realm, () => {
			const object = { name: "value" };
			Object.defineProperty(object, "name", {
				enumerable: true,
				get() { throw new Error("interceptor invoked"); },
			});
			return object;
		});
		assert.deepStrictEqual(result, { name: undefined });
	});

	await describe("indexed getter", { expectFailure: "interceptor invoked" }, async () => {
		await using agent = await Agent.create();
		const realm = await agent.createRealm();
		const result = await unsafeEvalAsStringInRealm(agent, realm, () => {
			const array = [ 0, 1 ];
			Object.defineProperty(array, 0, {
				enumerable: true,
				get() { throw new Error("interceptor invoked"); },
			});
			return array;
		});
		assert.deepStrictEqual(result, [ undefined, 1 ]);
	});
}
