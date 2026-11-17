import { AbortController, AbortSignal } from "@isolated-vm/runtime/dom/abort";
import { exposeInterface } from "@isolated-vm/runtime/webidl";

exposeInterface(AbortController);
exposeInterface(AbortSignal);
