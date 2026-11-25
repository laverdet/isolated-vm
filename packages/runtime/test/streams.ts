import { test } from "node:test";
import { makeTestAgent } from "@isolated-vm/experimental/test/fixtures";

// The realm has the streams runtime installed
const options = {
	packages: import.meta,
	install: `
		import "@isolated-vm/intrinsics/install";
		import "@isolated-vm/runtime/dom/install/event";
		import "@isolated-vm/runtime/dom/install/abort";
		import "@isolated-vm/runtime/streams/install";`,
};

await test("streams: readable", async () => {
	await using vm = await makeTestAgent(options);
	await vm.unsafeModule(async () => {
		const stream = new ReadableStream<number>({
			start(controller) { controller.enqueue(1); controller.enqueue(2); },
			pull(controller) { controller.enqueue(3); controller.close(); },
		});
		assert.deepEqual(await Array.fromAsync(stream), [ 1, 2, 3 ]);
	});
});

await test("streams: pipeline", async () => {
	await using vm = await makeTestAgent(options);
	await vm.unsafeModule(async () => {
		const source = new ReadableStream<string>({
			start(controller) { controller.enqueue("a"); controller.enqueue("b"); controller.close(); },
		});
		const upper = new TransformStream<string, string>({
			transform(chunk, controller) { controller.enqueue(chunk.toUpperCase()); },
		});
		const written: string[] = [];
		const sink = new WritableStream<string>({ write(chunk) { written.push(chunk); } });
		await source.pipeThrough(upper).pipeTo(sink);
		assert.deepEqual(written, [ "A", "B" ]);
	});
});
