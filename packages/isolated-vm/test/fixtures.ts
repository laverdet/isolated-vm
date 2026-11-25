// eslint-disable-next-line @typescript-eslint/triple-slash-reference -- For `@isolated/runtime` tests
/// <reference path="./rt.d.ts" preserve="true" />
import type { AbstractModule, Module, Realm, Reference, Script } from "@isolated-vm/experimental";
import { afterEach } from "node:test";
import { Agent, Capability, expect, expectComplete } from "@isolated-vm/experimental";
import { makeCompositeLinker, makeDirectResolver, makeFileSystemCompilationLoader, makeLinker, makeLocalResolver, makeStaticLoader } from "@isolated-vm/experimental/utility/linker";

export { expect, expectComplete, expectThrow } from "@isolated-vm/experimental/utility/completion";

/** Bun runs every test file in one process, can't transfer `SharedArrayBuffer`, and has no `node:zlib` for the addon */
export const isBun: boolean = "isBun" in process;

export interface TestAgentOptions extends Agent.CreateOptions {
	/** Capability modules, keyed by name. `{ name }` is imported as `"test:name"`. */
	capabilities?: Record<string, Realm.CapabilityInterface>;
	/** Module source evaluated once, after `rt` and `assert` are installed */
	install?: string;
	/** Compiles `@isolated-vm/*` packages from the file system, resolved as this module would resolve them */
	packages?: ImportMeta;
}

export interface UnsafeModuleOptions extends Agent.CompileModuleOptions, Module.EvaluateOptions {
	/** Module source placed before the function, for its imports */
	prelude?: string;
}

/**
 * Testing `Agent` utilities. One `Realm` is included, along with runtime and assert support.
 */
export class TestAgent implements AsyncDisposable {
	readonly agent: Agent;
	readonly realm: Realm;
	readonly linker: Module.Linker;
	readonly #global;
	readonly #stack;

	constructor(stack: AsyncDisposableStack, agent: Agent, realm: Realm, linker: Module.Linker, global: Reference) {
		this.agent = agent;
		this.realm = realm;
		this.linker = linker;
		this.#global = global;
		this.#stack = stack;
	}

	/**
	 * Runs a host function from its source text in a script. "unsafe" because you shouldn't be
	 * stringifying functions like this. Resolves `null` if the script was terminated.
	 */
	unsafeScript<Args extends unknown[], Type>(code: (...args: Args) => Type, ...args: Args): Promise<Type>;
	unsafeScript<Args extends unknown[], Type>(options: Script.RunScriptOptions, code: (...args: Args) => Type, ...args: Args): Promise<Type | null>;
	async unsafeScript(first: Script.RunScriptOptions | ((...args: never[]) => unknown), ...rest: unknown[]): Promise<unknown> {
		const options = typeof first === "function" ? undefined : first;
		const [ code, ...args ] = typeof first === "function" ? [ first, ...rest ] : rest;
		const source = `(${String(code)})(${args.map(arg => JSON.stringify(arg)).join(",")})`;
		const script = expectComplete(await this.agent.compileScript(source));
		const completion = await script.run(this.realm, options);
		return completion === null ? null : expectComplete(completion);
	}

	/**
	 * Runs a host function from its source text in a module. It can return a promise which is
	 * resolved through a capability.
	 */
	unsafeModule<Args extends unknown[], Result>(code: (...args: Args) => Result, ...args: Args): Promise<Result>;
	unsafeModule<Args extends unknown[], Result>(prelude: string, code: (...args: Args) => Result, ...args: Args): Promise<Result>;
	unsafeModule<Args extends unknown[], Result>(options: UnsafeModuleOptions, code: (...args: Args) => Result, ...args: Args): Promise<Result | null>;
	async unsafeModule(first: string | UnsafeModuleOptions | ((...args: never[]) => unknown), ...rest: unknown[]): Promise<unknown> {
		const { prelude = "", origin, timeout } = typeof first === "string"
			? { prelude: first }
			: typeof first === "object" ? first : {};
		const [ code, ...args ] = typeof first === "function" ? [ first, ...rest ] : rest;
		const { promise, resolve, reject } = Promise.withResolvers<unknown>();
		// `notify` may reject during evaluation, before the caller has the promise
		void promise.catch(() => undefined);
		const notify = {
			default: Capability.void((error: unknown, result: unknown) => {
				if (error === null) {
					resolve(result);
				} else {
					reject(new Error(error as string));
				}
			}),
		};
		const capability = expect(await this.realm.createCapability(notify, { origin: "test:notify" }));
		const module = expectComplete(await this.agent.compileModule(`
			${prelude}
			import notify from "test:notify";
			void async function() {
				try {
					notify(null, await (${String(code)})(${args.map(arg => JSON.stringify(arg)).join(",")}));
				} catch (error) {
					notify(error instanceof Error ? error.stack : String(error));
				}
			}();
		`, origin === undefined ? undefined : { origin }));
		const linker = makeLinker(makeDirectResolver(), makeStaticLoader({ "test:notify": capability }));
		await module.link(this.realm, makeCompositeLinker(linker, this.linker));
		const completion = await module.evaluate(this.realm, { timeout });
		if (completion === null) {
			// Terminated, so `notify` will never be invoked
			return null;
		}
		expectComplete(completion);
		return promise;
	}

	/** Returns a reference to a host function defined in the realm from its source, so it can't close over anything */
	async defineFunction<Fn extends (...args: never[]) => unknown>(code: Fn): Promise<Reference<Fn>> {
		const script = expectComplete(await this.agent.compileScript(`new rt.Reference(${String(code)})`));
		return expectComplete(await script.run(this.realm)) as Reference<Fn>;
	}

	/** Returns a reference to `globalThis[name]` */
	async get(name: string): Promise<Reference> {
		return expect(await this.#global.get(name));
	}

	/** Copies `globalThis[name]` */
	async copy(name: string): Promise<unknown> {
		const reference = await this.get(name);
		return reference.copy();
	}

	/** Assigns `globalThis[name]` */
	async set(name: string, value: unknown): Promise<void> {
		expect(await this.#global.set(name, value));
	}

	async [Symbol.asyncDispose](): Promise<void> {
		return this.#stack.disposeAsync();
	}
}

/** Creates an agent and a realm, with the modules requested in `options` ready to link */
export async function makeTestAgent(options: TestAgentOptions = {}): Promise<TestAgent> {
	await using stack = new AsyncDisposableStack();
	const { capabilities, install, packages, ...createOptions } = options;
	const agent = stack.use(await Agent.create(createOptions));
	const realm = expect(await agent.createRealm());
	const handles = new Set<NodeJS.Timeout>();
	stack.defer(() => {
		for (const handle of handles) {
			clearTimeout(handle);
		}
	});
	let wake: Reference<() => void> | undefined;
	const modules: Record<string, AbstractModule> = {
		"isolated-vm:runtime": expect(await realm.instantiateRuntime()),
		"isolated-vm:capability/timers": expect(await realm.createCapability(
			{
				initialize: Capability.void((reference: unknown) => {
					wake = reference as Reference<() => void>;
				}),
				schedule: Capability.void((timeout: unknown) => {
					const handle = setTimeout(() => {
						handles.delete(handle);
						void wake?.invoke([]);
					}, Math.max(0, Number(timeout) - Date.now()));
					handles.add(handle);
				}),
			},
			{ origin: "isolated-vm:capability/timers" })),
	};
	for (const [ name, capability ] of Object.entries(capabilities ?? {})) {
		const specifier = `test:${name}`;
		modules[specifier] = expect(await realm.createCapability(capability, { origin: specifier }));
	}
	const linkers = [ makeLinker(makeDirectResolver(), makeStaticLoader(modules)) ];
	if (packages) {
		linkers.push(makeLinker(makeLocalResolver(packages), makeFileSystemCompilationLoader(agent)));
	}
	const linker = makeCompositeLinker(...linkers);
	const rt = expectComplete(await agent.compileModule(`
		import * as rt from "isolated-vm:runtime";
		globalThis.rt = rt;
		globalThis.assert = (${String(makeAssert)})();
		${install ?? ""}`));
	await rt.link(realm, linker);
	expectComplete(await rt.evaluate(realm));
	return new TestAgent(stack.move(), agent, realm, linker, await realm.acquireGlobalObject());
}

/** Source of the realm's `Assert`. This runs in the realm from its source text, so it can't close over anything. */
function makeAssert() {
	const inspect = (value: unknown) => {
		try {
			const plain = value === undefined || typeof value === "function" || typeof value === "symbol";
			return plain || value instanceof RegExp ? String(value) : JSON.stringify(value);
		} catch {
			return String(value);
		}
	};
	const fail = (actual: unknown, relation: string, expected: unknown): never => {
		throw new Error(`Expected ${inspect(actual)} ${relation} ${inspect(expected)}`);
	};
	const isDeepEqual = (actual: unknown, expected: unknown): boolean => {
		if (Object.is(actual, expected)) {
			return true;
		} else if (
			typeof actual !== "object" || typeof expected !== "object" ||
			actual === null || expected === null ||
			Object.getPrototypeOf(actual) !== Object.getPrototypeOf(expected)
		) {
			return false;
		}
		const keys = Object.keys(actual);
		return keys.length === Object.keys(expected).length && keys.every(key =>
			key in expected &&
			isDeepEqual((actual as Record<string, unknown>)[key], (expected as Record<string, unknown>)[key]));
	};
	type Expected = RegExp | Record<string, unknown> | (abstract new (...args: never[]) => unknown);
	const expectThrown = (error: unknown, expected?: Expected) => {
		if (expected instanceof RegExp) {
			if (!expected.test(String(error))) {
				fail(error, "to match", expected);
			}
		} else if (typeof expected === "function") {
			if (!(error instanceof expected)) {
				fail(error, "to be an instance of", expected.name);
			}
		} else if (expected !== undefined) {
			for (const [ key, value ] of Object.entries(expected)) {
				const actual = (error as Record<string, unknown>)[key];
				if (value instanceof RegExp ? !value.test(String(actual)) : !isDeepEqual(actual, value)) {
					fail(actual, `at "${key}" to equal`, value);
				}
			}
		}
	};
	return {
		ok(value: unknown): asserts value {
			if (!value) {
				fail(value, "to be", "truthy");
			}
		},
		equal<Type>(actual: unknown, expected: Type): asserts actual is Type {
			if (!Object.is(actual, expected)) {
				fail(actual, "to equal", expected);
			}
		},
		notEqual(actual: unknown, expected: unknown) {
			if (Object.is(actual, expected)) {
				fail(actual, "to not equal", expected);
			}
		},
		deepEqual<Type>(actual: unknown, expected: Type): asserts actual is Type {
			if (!isDeepEqual(actual, expected)) {
				fail(actual, "to deeply equal", expected);
			}
		},
		throws(fn: () => unknown, expected?: Expected): void {
			try {
				fn();
			} catch (error) {
				expectThrown(error, expected);
				return;
			}
			fail(fn, "to throw", typeof expected === "function" ? expected.name : expected);
		},
		async rejects(promise: Promise<unknown>, expected?: Expected): Promise<void> {
			try {
				await promise;
			} catch (error) {
				expectThrown(error, expected);
				return;
			}
			fail(promise, "to reject", typeof expected === "function" ? expected.name : expected);
		},
	};
}

/** @internal */
export function unsafeIIFEAsString(
	code: () => unknown,
) {
	return `(${String(code)})()`;
}

/** @internal */
export function clearUncaughtExceptionListeners(): Disposable {
	const listeners = process.rawListeners("uncaughtException") as NodeJS.UncaughtExceptionListener[];
	process.removeAllListeners("uncaughtException");
	return {
		[Symbol.dispose]() {
			process.removeAllListeners("uncaughtException");
			for (const listener of listeners) {
				process.on("uncaughtException", listener);
			}
		},
	};
}

// Run garbage collection after each test. This cleans up internal napi externals.
afterEach(() => {
	global.gc?.();
});
