// @ts-check
import { fileURLToPath } from "node:url";
import path from "@isolated-vm/experimental/backend/specifier";

console.log(fileURLToPath(import.meta.resolve(path)));
