import * as assert from "node:assert/strict";
import { describe, test } from "node:test";
import * as ivm from "@isolated-vm/experimental";
import { unsafeEvalAsStringInRealm } from "./fixtures.js";

async function check(agent: ivm.Agent, realm: ivm.Realm, fn: () => unknown) {
	const result = await unsafeEvalAsStringInRealm(agent, realm, fn);
	assert.deepStrictEqual(result, fn());
}

await test("transfer types", async () => {
	await using agent = await ivm.Agent.create();
	const realm = await agent.createRealm();

	// null, undefined, bool
	await check(agent, realm, () => null);
	await check(agent, realm, () => undefined);
	await check(agent, realm, () => true);
	await check(agent, realm, () => false);

	// string
	await check(agent, realm, () => "hello world");
	await check(agent, realm, () => "\u{10ffff}");

	// number
	await check(agent, realm, () => 0);
	await check(agent, realm, () => Number.MAX_SAFE_INTEGER);
	await check(agent, realm, () => Number.MAX_SAFE_INTEGER + 1);
	await check(agent, realm, () => Number.MIN_SAFE_INTEGER);
	await check(agent, realm, () => Number.MIN_SAFE_INTEGER - 1);
	await check(agent, realm, () => Math.PI);

	// bigint
	await check(agent, realm, () => 0n);
	await check(agent, realm, () => 9_223_372_036_854_775_807n); // int64 max
	await check(agent, realm, () => 9_223_372_036_854_775_808n); // int64 max + 1
	await check(agent, realm, () => -9_223_372_036_854_775_808n); // int64 min
	await check(agent, realm, () => -9_223_372_036_854_775_809n); // int64 min - 1
	await check(agent, realm, () => 18_446_744_073_709_551_615n); // uint64 max
	await check(agent, realm, () => 18_446_744_073_709_551_616n); // uint64 max + 1
	await check(agent, realm, () => 502_360_950_888_298_027_355_518_043_327_124_471_675_959_709_617_316_205_938_938_264_006_428_765_255_489n); // 256 bit number
	await check(agent, realm, () => -502_360_950_888_298_027_355_518_043_327_124_471_675_959_709_617_316_205_938_938_264_006_428_765_255_490n); // 256 bit negative
});

// None of these work!
// await describe("interceptors", async () => {
// 	await using agent = await ivm.Agent.create();
// 	const realm = await agent.createRealm();

// 	await test("named proxy", async () => {
// 		const result = await unsafeEvalAsStringInRealm(agent, realm, () => {
// 			const proxy = new Proxy({ name: "value" }, {
// 				get() { throw new Error("interceptor invoked"); },
// 			});
// 			return proxy;
// 		});
// 		assert.deepStrictEqual(result, { name: undefined });
// 	});

// 	await test("indexed proxy", async () => {
// 		const result = await unsafeEvalAsStringInRealm(agent, realm, () => {
// 			const proxy = new Proxy([ 0, 1 ], {
// 				get() { throw new Error("interceptor invoked"); },
// 			});
// 			return proxy;
// 		});
// 		assert.deepStrictEqual(result, [ undefined, undefined ]);
// 	});

// 	await describe("named getter", async () => {
// 		const result = await unsafeEvalAsStringInRealm(agent, realm, () => {
// 			const object = { name: "value" };
// 			Object.defineProperty(object, "name", {
// 				enumerable: true,
// 				get() { throw new Error("interceptor invoked"); },
// 			});
// 			return object;
// 		});
// 		assert.deepStrictEqual(result, { name: undefined });
// 	});

// 	await describe("indexed getter", async () => {
// 		const result = await unsafeEvalAsStringInRealm(agent, realm, () => {
// 			const array = [ 0, 1 ];
// 			Object.defineProperty(array, 0, {
// 				enumerable: true,
// 				get() { throw new Error("interceptor invoked"); },
// 			});
// 			return array;
// 		});
// 		assert.deepStrictEqual(result, [ undefined, 1 ]);
// 	});
// });

await describe("regressions", async () => {
	await using agent = await ivm.Agent.create();

	await test("numeric references", async () => {
		const realm = await agent.createRealm();
		const global = await realm.acquireGlobalObject();
		// doubles are stored as references in `value_t`, because they are bigger than 32-bit. there was
		// a failure with more than one of them in the same transfer operation.
		await global.set("xx", { zero: 0.1, one: 0.2 });
	});

	await test("numeric indices", async () => {
		const realm = await agent.createRealm();
		const global = await realm.acquireGlobalObject();
		// numeric array indices broke as part of a double to int32_t change.
		await global.set("xx", [ false, true ]);
	});

	await test("seen index type", async () => {
		const realm = await agent.createRealm();
		const global = await realm.acquireGlobalObject();
		const payload = {
			number: 0,
			buffer: [ new Uint8Array(new SharedArrayBuffer(0)) ],
		};
		await global.set("xx", [ payload ]);
	});
});
