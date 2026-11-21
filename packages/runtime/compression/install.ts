import { CompressionStream, DecompressionStream } from "@isolated-vm/runtime/compression";
import { exposeInterface } from "@isolated-vm/runtime/webidl";

exposeInterface(CompressionStream);
exposeInterface(DecompressionStream);
