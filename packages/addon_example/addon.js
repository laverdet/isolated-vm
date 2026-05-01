import { fileURLToPath } from "node:url";
import { Agent, NativeModule, expectComplete } from "@isolated-vm/experimental";

const addon = await NativeModule.create(fileURLToPath(new URL("build/addon_example", import.meta.url)), "module:example");
const agent = await Agent.create();
const realm = await agent.createRealm();
const native = await addon.instantiate(realm);
const module = expectComplete(await agent.compileModule('import { one } from "module:example"; globalThis.result = one;'));
await module.link(realm, () => native);
expectComplete(await module.evaluate(realm));
const global = await realm.acquireGlobalObject();
const result = await global.get("result");
console.log(await result.copy());
