import { equal, throws } from "node:assert/strict";
import { test } from "node:test";
import { Capability } from "@isolated-vm/experimental";
import { clearUncaughtExceptionListeners, expectComplete, isBun, makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

const supportsUncaughtTest = { skip: isBun && "nodejs-specific" };

await test("sync capability resolves", async () => {
	const add = (left: unknown, right: unknown) => Promise.resolve(Number(left) + Number(right));
	await using vm = await makeTestAgent({ capabilities: { add: { default: Capability.sync(add) } } });
	await vm.unsafeModule('import add from "test:add";', () => {
		assert.equal(add(1, 2), 3);
	});
});

await test("sync capability rejects", async () => {
	const fail = () => Promise.reject(new Error("nope"));
	// eslint-disable-next-line @typescript-eslint/prefer-promise-reject-errors
	const uninspectable = () => Promise.reject(Symbol("reason"));
	await using vm = await makeTestAgent({
		capabilities: {
			fail: { default: Capability.sync(fail) },
			uninspectable: { default: Capability.sync(uninspectable) },
		},
	});
	await vm.unsafeModule('import fail from "test:fail"; import uninspectable from "test:uninspectable";', () => {
		assert.throws(fail, { message: "nope" });
		assert.throws(uninspectable, Error);
	});
});

await test("sync capability throws", async () => {
	const fail = () => { throw new Error("nope"); };
	await using vm = await makeTestAgent({ capabilities: { fail: { default: Capability.sync(fail) } } });
	await vm.unsafeModule('import fail from "test:fail";', () => {
		assert.throws(fail, { message: "nope" });
	});
});

await test("void capability throws as uncaught exception", supportsUncaughtTest, async () => {
	using restore = clearUncaughtExceptionListeners();
	const uncaught = new Promise<string>(resolve => {
		process.once("uncaughtException", error => {
			gc?.();
			resolve(error.message);
		});
	});
	const fail = () => { throw new Error("nope"); };
	await using vm = await makeTestAgent({ capabilities: { fail: { default: Capability.void(fail) } } });
	for (let ii = 0; ii < 100; ++ii) await vm.get("Object");
	await vm.unsafeModule('import fail from "test:fail";', () => { fail(); });
	equal(await uncaught, "nope");
});

await test("abandoned sync capability resolves null", async () => {
	const abandoned = () => new Promise(() => void setTimeout(() => global.gc?.(), 10));
	await using vm = await makeTestAgent({ capabilities: { abandoned: { default: Capability.sync(abandoned) } } });
	await vm.unsafeModule('import abandoned from "test:abandoned";', () => {
		assert.equal(abandoned(), null);
	});
});

await test("capability is one-shot", async () => {
	const once = { default: Capability.sync(() => {}) };
	await using vm = await makeTestAgent({ capabilities: { once } });
	throws(() => vm.realm.createCapability(once, { origin: "test:once" }), /already used/);
});

await test("suspended tasks run concurrently", async () => {
	const gates = [ 0, 1, 2 ].map(() => Promise.withResolvers<number>());
	const suspensions = [ 0, 1, 2 ].map(() => Promise.withResolvers<null>());
	const wait = (index: number) => {
		suspensions[index]?.resolve(null);
		return gates[index]?.promise;
	};
	await using vm = await makeTestAgent({ capabilities: { wait: { default: Capability.sync(wait) } } });
	const prelude = 'import wait from "test:wait";';
	const bump = () => {
		const global = globalThis as unknown as { marker?: number };
		global.marker = (global.marker ?? 0) + 1;
	};
	// Two modules suspend at once, then a script runs on a third thread while both are waiting
	const first = vm.unsafeModule(prelude, () => {
		assert.deepEqual([ wait(0), wait(2) ], [ 10, 30 ]);
		return (globalThis as unknown as { marker: number }).marker;
	});
	const second = vm.unsafeModule(prelude, () => { assert.equal(wait(1), 20); });
	await Promise.all([ suspensions[0]?.promise, suspensions[1]?.promise ]);
	await vm.unsafeScript(bump);
	gates[1]?.resolve(20);
	await second;
	gates[0]?.resolve(10);
	await suspensions[2]?.promise;
	await vm.unsafeScript(bump);
	gates[2]?.resolve(30);
	// Both scripts ran before the first module resumed
	equal(await first, 2);
});

await test("dispose while suspended", async () => {
	const forever = () => new Promise(() => void vm.agent.disposeAsync());
	const vm = await makeTestAgent({ capabilities: { forever: { default: Capability.sync(forever) } } });
	const module = expectComplete(await vm.agent.compileModule('import forever from "test:forever"; forever();'));
	await module.link(vm.realm, vm.linker);
	// The evaluation settles even though the agent is gone before `forever()` could return
	await module.evaluate(vm.realm);
});
