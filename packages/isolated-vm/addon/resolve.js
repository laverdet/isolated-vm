// @ts-check
import { fileURLToPath } from "node:url";
import host from "#host_cjs";

console.log(fileURLToPath(import.meta.resolve(`@isolated-vm/experimental-${host}`)));
