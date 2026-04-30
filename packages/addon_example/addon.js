import { fileURLToPath } from "node:url";
import { Agent, NativeModule } from "@isolated-vm/experimental";

const addon = await NativeModule.create(fileURLToPath(new URL("build/addon_example", import.meta.url)));
const agent = await Agent.create();
await addon.instantiate(agent);
