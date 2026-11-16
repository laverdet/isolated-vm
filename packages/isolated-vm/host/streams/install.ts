import { ByteLengthQueuingStrategy, CountQueuingStrategy, ReadableStream, ReadableStreamDefaultController, ReadableStreamDefaultReader, TransformStream, TransformStreamDefaultController, WritableStream, WritableStreamDefaultController, WritableStreamDefaultWriter } from "@isolated-vm/experimental/host/streams";
import { exposeInterface } from "@isolated-vm/experimental/host/webidl";

exposeInterface(ByteLengthQueuingStrategy);
exposeInterface(CountQueuingStrategy);
exposeInterface(ReadableStream);
exposeInterface(ReadableStreamDefaultController);
exposeInterface(ReadableStreamDefaultReader);
exposeInterface(TransformStream);
exposeInterface(TransformStreamDefaultController);
exposeInterface(WritableStream);
exposeInterface(WritableStreamDefaultController);
exposeInterface(WritableStreamDefaultWriter);
