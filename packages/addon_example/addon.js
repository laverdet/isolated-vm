import { fileURLToPath } from "node:url";
import { NativeModule } from "@isolated-vm/experimental";

const addon = await NativeModule.create(fileURLToPath(new URL("build/addon_example", import.meta.url)));
export default addon;
