import * as assert from "node:assert/strict";
import { test } from "node:test";
import { Agent, Capability, expect } from "@isolated-vm/experimental";
import { expectComplete } from "@isolated-vm/experimental/test/fixtures";
import { makeCompositeLinker, makeDirectResolver, makeFileSystemCompilationLoader, makeLinker, makeLocalResolver, makeStaticLoader } from "@isolated-vm/experimental/utility/linker";

/** Installs the streams runtime in a fresh realm and runs `body`, which reports via `notify()`. */
async function runStreams(body: string) {
	await using agent = await Agent.create();
	const realm = expect(await agent.createRealm());
	const resolvers = Promise.withResolvers<unknown>();
	const capabilities = makeStaticLoader({
		"isolated-vm:runtime": expect(await realm.instantiateRuntime()),
		// `AbortSignal.timeout` links against the timers capability, though nothing here schedules
		"isolated-vm:capability/timers": expect(await realm.createCapability(
			{
				initialize: Capability.void(() => {}),
				schedule: Capability.void(() => {}),
			},
			{ origin: "isolated-vm:capability/timers" })),
		"notify-test": expect(await realm.createCapability(
			{
				default: Capability.void((message: unknown) => { resolvers.resolve(message); }),
			},
			{ origin: "notify-test" })),
	});
	const linker = makeCompositeLinker(
		makeLinker(makeDirectResolver(), capabilities),
		makeLinker(makeLocalResolver(import.meta), makeFileSystemCompilationLoader(agent)));
	const module = expectComplete(await agent.compileModule(`
		import "@isolated-vm/intrinsics/install";
		import "@isolated-vm/runtime/dom/install/event";
		import "@isolated-vm/runtime/dom/install/abort";
		import "@isolated-vm/runtime/streams/install";
		import notify from "notify-test";
		(async () => { ${body} })().then(result => notify(JSON.stringify(result)), error => notify(JSON.stringify({ error: String(error) })));
	`));
	await module.link(realm, linker);
	await module.evaluate(realm);
	const timeout = setTimeout(() => resolvers.reject(new Error("Timed out")), 5000);
	try {
		return JSON.parse(String(await resolvers.promise)) as unknown;
	} finally {
		clearTimeout(timeout);
	}
}

await test("streams: interface shape", async () => {
	const result = await runStreams(`
		const names = [ "ByteLengthQueuingStrategy", "CountQueuingStrategy", "ReadableStream", "ReadableStreamDefaultController", "ReadableStreamDefaultReader", "TransformStream", "TransformStreamDefaultController", "WritableStream", "WritableStreamDefaultController", "WritableStreamDefaultWriter" ];
		const rs = new ReadableStream();
		return {
			globals: names.map(name => typeof globalThis[name]),
			enumerable: Object.getOwnPropertyDescriptor(ReadableStream.prototype, "getReader").enumerable,
			toStringTag: Object.prototype.toString.call(rs),
			asyncIterator: ReadableStream.prototype[Symbol.asyncIterator] === ReadableStream.prototype.values,
			illegalConstructor: [ "ReadableStreamDefaultController", "WritableStreamDefaultController", "TransformStreamDefaultController" ]
				.map(name => { try { new globalThis[name](); return false; } catch (error) { return error instanceof TypeError; } }),
			lengths: [ ReadableStream.prototype.pipeTo.length, ReadableStream.prototype.pipeThrough.length, WritableStream.length, new CountQueuingStrategy({ highWaterMark: 1 }).size.length ],
			brandRejection: await ReadableStream.prototype.cancel.call({}).then(() => "resolved", error => error.constructor.name),
			byteStreams: (() => { try { new ReadableStream({ type: "bytes" }); return false; } catch (error) { return error instanceof TypeError; } })(),
		};
	`);
	assert.deepEqual(result, {
		globals: Array.from({ length: 10 }, () => "function"),
		enumerable: true,
		toStringTag: "[object ReadableStream]",
		asyncIterator: true,
		illegalConstructor: [ true, true, true ],
		lengths: [ 1, 1, 0, 0 ],
		brandRejection: "TypeError",
		byteStreams: true,
	});
});

await test("streams: readable", async () => {
	const result = await runStreams(`
		const make = () => new ReadableStream({
			start(controller) { controller.enqueue(1); controller.enqueue(2); },
			pull(controller) { controller.enqueue(3); controller.close(); },
		});
		const reader = make().getReader();
		const reads = [];
		for (let ii = 0; ii < 4; ++ii) { reads.push(await reader.read()); }
		const iterated = [];
		for await (const chunk of make()) { iterated.push(chunk); }
		const [ left, right ] = make().tee();
		const teed = await Promise.all([ Array.fromAsync(left), Array.fromAsync(right) ]);
		const from = await Array.fromAsync(ReadableStream.from([ "a", "b" ]));
		const fromAsync = await Array.fromAsync(ReadableStream.from((async function*() { yield "x"; yield "y"; })()));
		let cancelReason;
		const cancelable = new ReadableStream({ cancel(reason) { cancelReason = reason; } });
		await cancelable.cancel("why");
		const strategy = new ReadableStream({ start(controller) { controller.enqueue(new Uint8Array(3)); } }, new ByteLengthQueuingStrategy({ highWaterMark: 10 }));
		const { value } = await strategy.getReader().read();
		return { reads, iterated, teed, from, fromAsync, cancelReason, locked: [ cancelable.locked, make().getReader() && true ], strategy: value.byteLength };
	`);
	assert.deepEqual(result, {
		reads: [ { value: 1, done: false }, { value: 2, done: false }, { value: 3, done: false }, { done: true } ],
		iterated: [ 1, 2, 3 ],
		teed: [ [ 1, 2, 3 ], [ 1, 2, 3 ] ],
		from: [ "a", "b" ],
		fromAsync: [ "x", "y" ],
		cancelReason: "why",
		locked: [ false, true ],
		strategy: 3,
	});
});

await test("streams: writable", async () => {
	const result = await runStreams(`
		const events = [];
		let signalReason;
		const ws = new WritableStream({
			start(controller) { events.push("start"); controller.signal.addEventListener("abort", () => { signalReason = controller.signal.reason; }); },
			write(chunk) { events.push([ "write", chunk ]); },
			close() { events.push("close"); },
		}, new CountQueuingStrategy({ highWaterMark: 2 }));
		const writer = ws.getWriter();
		const desiredSize = writer.desiredSize;
		await writer.write("a");
		await writer.write("b");
		await writer.close();
		const closed = await writer.closed.then(() => "closed");
		const aborted = new WritableStream({ abort(reason) { events.push([ "abort", reason ]); } });
		await aborted.abort("stop");
		const afterAbort = await aborted.getWriter().write("x").catch(reason => reason);
		const erroring = new WritableStream({ write() { throw new Error("boom"); } });
		const erroringWriter = erroring.getWriter();
		const writeError = await erroringWriter.write("x").catch(error => error.message);
		return { events, desiredSize, closed, afterAbort, writeError, signalReason: signalReason === undefined ? "none" : signalReason };
	`);
	assert.deepEqual(result, {
		events: [ "start", [ "write", "a" ], [ "write", "b" ], "close", [ "abort", "stop" ] ],
		desiredSize: 2,
		closed: "closed",
		afterAbort: "stop",
		writeError: "boom",
		signalReason: "none",
	});
});

await test("streams: transform & piping", async () => {
	const result = await runStreams(`
		const upper = new TransformStream({ transform(chunk, controller) { controller.enqueue(chunk.toUpperCase()); }, flush(controller) { controller.enqueue("!"); } });
		const piped = await Array.fromAsync(ReadableStream.from([ "a", "b" ]).pipeThrough(upper));
		const written = [];
		const sink = new WritableStream({ write(chunk) { written.push(chunk); } });
		await ReadableStream.from([ 1, 2, 3 ]).pipeTo(sink);
		const closedSink = await sink.getWriter().closed.then(() => "closed");
		const controller = new AbortController();
		const never = new ReadableStream({ pull() { return new Promise(() => {}); } });
		const pending = never.pipeTo(new WritableStream(), { signal: controller.signal });
		controller.abort("halt");
		const abortReason = await pending.catch(reason => reason);
		const lockedResult = await (() => { const rs = ReadableStream.from([]); rs.getReader(); return rs.pipeTo(new WritableStream()); })().catch(error => error.constructor.name);
		const errored = new ReadableStream({ start(controller) { controller.error(new Error("source")); } });
		const propagated = await errored.pipeTo(new WritableStream()).catch(error => error.message);
		const terminated = new TransformStream({ transform(chunk, controller) { controller.terminate(); } });
		const terminatedWriter = terminated.writable.getWriter();
		// The readable side has a high water mark of 0, so the write waits for a read
		const terminatedWrite = terminatedWriter.write("x").then(() => "resolved", error => error.constructor.name);
		const readableDone = await Array.fromAsync(terminated.readable);
		const terminatedClosed = await terminatedWriter.closed.then(() => "closed", error => error.constructor.name);
		return { piped, written, closedSink, abortReason, lockedResult, propagated, terminatedWrite: await terminatedWrite, terminatedClosed, readableDone };
	`);
	assert.deepEqual(result, {
		piped: [ "A", "B", "!" ],
		written: [ 1, 2, 3 ],
		closedSink: "closed",
		abortReason: "halt",
		lockedResult: "TypeError",
		propagated: "source",
		terminatedWrite: "resolved",
		terminatedClosed: "TypeError",
		readableDone: [],
	});
});
