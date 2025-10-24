import type { MaybeCompletionOf } from "backend_v8.node";
import * as assert from "node:assert/strict";
import { Agent, Realm } from "isolated-vm";

/** @internal */
export function unwrapCompletion<Type>(completion: MaybeCompletionOf<Type>): Type {
	assert.ok(completion);
	assert.ok(completion.complete);
	return completion.result;
}

/** @internal */
export function unwrapThrowCompletion(completion: MaybeCompletionOf<unknown>): unknown {
	assert.ok(completion);
	assert.ok(!completion.complete);
	return completion.error;
}

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
	const script = unwrapCompletion(await agent.compileScript(`(${String(code)})(${args.map(arg => JSON.stringify(arg)).join(",")})`));
	return unwrapCompletion(await script.run(realm)) as Type;
}
