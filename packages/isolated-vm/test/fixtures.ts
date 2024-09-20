import { Agent, Realm } from "isolated-vm";

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
	const script = await agent.compileScript(`(${String(code)})(${args.map(arg => JSON.stringify(arg)).join(",")})`);
	return await script.run(realm) as Type;
}
