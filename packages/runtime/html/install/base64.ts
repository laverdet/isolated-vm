import { atob, btoa } from "@isolated-vm/runtime/html/base64";

globalThis.atob = atob;
globalThis.btoa = btoa;
