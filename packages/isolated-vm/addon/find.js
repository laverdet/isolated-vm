// @ts-check
import { fileURLToPath } from "node:url";
import path from "@isolated-vm/experimental/backend_path";

console.log(fileURLToPath(import.meta.resolve(path)));
