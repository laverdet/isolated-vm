import { afterEach } from "node:test";
import { Agent, Realm } from "@isolated-vm/experimental";
import { expectComplete } from "@isolated-vm/experimental/utility/completion";

export { expectComplete, expectThrow } from "@isolated-vm/experimental/utility/completion";

/** @internal */
export function unsafeIIFEAsString(
	code: () => unknown,
) {
	return `(${String(code)})()`;
}

/** @internal */
export async function unsafeEvalAsString<Type, Args extends unknown[]>(
	agent: Agent,
	code: (...args: Args) => Type,
	...args: Args
): Promise<Type> {
	const realm = await agent.createRealm();
	return unsafeEvalAsStringInRealm(agent, realm, code, ...args);
}

/** @internal */
export async function unsafeEvalAsStringInRealm<Type, Args extends unknown[]>(
	agent: Agent,
	realm: Realm,
	code: (...args: Args) => Type,
	...args: Args
): Promise<Type> {
	const script = expectComplete(await agent.compileScript(`(${String(code)})(${args.map(arg => JSON.stringify(arg)).join(",")})`));
	return expectComplete(await script.run(realm)) as Type;
}

// Run garbage collection after each test. This cleans up internal napi externals.
afterEach(() => {
	global.gc?.();
});
