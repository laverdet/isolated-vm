import { AbortController, AbortSignal } from "@isolated-vm/experimental/host/dom/abort";
import { exposeInterface } from "@isolated-vm/experimental/host/webidl";

exposeInterface(AbortController);
exposeInterface(AbortSignal);
