import { test } from "node:test";
import { makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

await test("numeric references", async () => {
	await using vm = await makeTestAgent();
	// doubles are stored as references in `value_t`, because they are bigger than 32-bit. there was
	// a failure with more than one of them in the same transfer operation.
	await vm.set("xx", { zero: 0.1, one: 0.2 });
});

await test("numeric indices", async () => {
	await using vm = await makeTestAgent();
	// numeric array indices broke as part of a double to int32_t change.
	await vm.set("xx", [ false, true ]);
});

await test("seen index type", async () => {
	await using vm = await makeTestAgent();
	// `reference_map` is keyed by handle address, which for a smi is the value. `number: 0` was
	// remembered as a double, so array index 0 hit that entry and failed as a property key.
	await vm.set("xx", [ { number: 0, array: [ 0 ] } ]);
});
