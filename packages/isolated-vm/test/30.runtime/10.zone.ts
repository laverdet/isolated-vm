import { test } from "node:test";
import { makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

await test("zones nest and run returns the callback result", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeScript(() => {
		const outer = new rt.Zone();
		const inner = new rt.Zone();
		const result = outer.run(() => [
			rt.Zone.current === outer,
			inner.run(() => rt.Zone.current === inner),
			rt.Zone.current === outer,
		]);
		assert.deepEqual(result, [ true, true, true ]);
	});
});

await test("zone follows await and then", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeModule(async () => {
		const zone = new rt.Zone();
		const results: Record<string, boolean> = {};
		const ran = zone.run(async () => {
			// eslint-disable-next-line @typescript-eslint/await-thenable
			await undefined;
			results.awaited = rt.Zone.current === zone;
			await Promise.resolve().then(() => { results.chained = rt.Zone.current === zone; });
		});
		await Promise.resolve().then(() => { results.outside = rt.Zone.current === zone; });
		await ran;
		assert.deepEqual(results, { awaited: true, chained: true, outside: false });
	});
});

await test("clone snapshots data and detach runs in a new task", async () => {
	await using vm = await makeTestAgent();
	await vm.unsafeModule(async () => {
		const zone = new rt.Zone();
		zone.data.set("key", 1);
		const clone = zone.clone();
		zone.data.set("key", 2);
		clone.data.set("other", 3);
		assert.deepEqual(
			[ zone.data.get("key"), clone.data.get("key"), zone.data.has("other"), clone.data.has("other") ],
			[ 2, 1, false, true ]);
		const order: [ string, boolean ][] = [];
		await zone.run(async () => {
			const { promise, resolve } = Promise.withResolvers<undefined>();
			rt.Zone.detach(() => {
				order.push([ "detached", rt.Zone.current !== zone ]);
				resolve(undefined);
			});
			order.push([ "sync", rt.Zone.current === zone ]);
			await promise;
		});
		assert.deepEqual(order, [ [ "sync", true ], [ "detached", true ] ]);
	});
});
