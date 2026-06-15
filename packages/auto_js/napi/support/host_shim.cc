import std;

// NOLINTBEGIN(bugprone-macro-parentheses)
#if _WIN64
// I couldn't get this to work.
// https://stackoverflow.com/questions/2290587/gcc-style-weak-linking-in-visual-studio
#define SHIM(NAME)
#else
#define SHIM(NAME) \
	__attribute__((weak)) __attribute__((visibility("default"))) auto NAME = nullptr;
#endif
// NOLINTEND(bugprone-macro-parentheses)

extern "C" {
const void* shim_nullptr = nullptr;
// NOLINTBEGIN(bugprone-reserved-identifier)
SHIM(_ZN2v810Int16Array3NewENS_5LocalINS_17SharedArrayBufferEEEmm)
SHIM(_ZN2v810Int32Array3NewENS_5LocalINS_17SharedArrayBufferEEEmm)
SHIM(_ZN2v810Uint8Array3NewENS_5LocalINS_17SharedArrayBufferEEEmm)
SHIM(_ZN2v811Uint16Array3NewENS_5LocalINS_17SharedArrayBufferEEEmm)
SHIM(_ZN2v811Uint32Array3NewENS_5LocalINS_17SharedArrayBufferEEEmm)
SHIM(_ZN2v812BackingStoreD1Ev)
SHIM(_ZN2v812Float16Array3NewENS_5LocalINS_17SharedArrayBufferEEEmm)
SHIM(_ZN2v812Float32Array3NewENS_5LocalINS_17SharedArrayBufferEEEmm)
SHIM(_ZN2v812Float64Array3NewENS_5LocalINS_17SharedArrayBufferEEEmm)
SHIM(_ZN2v813BigInt64Array3NewENS_5LocalINS_17SharedArrayBufferEEEmm)
SHIM(_ZN2v814BigUint64Array3NewENS_5LocalINS_17SharedArrayBufferEEEmm)
SHIM(_ZN2v817SharedArrayBuffer15GetBackingStoreEv)
SHIM(_ZN2v817SharedArrayBuffer15NewBackingStoreEPvmPFvS1_mS1_ES1_)
SHIM(_ZN2v817SharedArrayBuffer3NewEPNS_7IsolateENSt3__110shared_ptrINS_12BackingStoreEEE)
SHIM(_ZN2v817Uint8ClampedArray3NewENS_5LocalINS_17SharedArrayBufferEEEmm)
SHIM(_ZN2v87Isolate10GetCurrentEv)
SHIM(_ZN2v88DataView3NewENS_5LocalINS_17SharedArrayBufferEEEmm)
SHIM(_ZN2v89Int8Array3NewENS_5LocalINS_17SharedArrayBufferEEEmm)
SHIM(_ZNK2v812BackingStore4DataEv)
SHIM(_ZNK2v817SharedArrayBuffer10ByteLengthEv)
SHIM(_ZNK2v85Value7IsInt32Ev)
SHIM(_ZNK2v85Value8IsNumberEv)
SHIM(_ZNK2v86String9IsOneByteEv)
SHIM(node_api_is_sharedarraybuffer)
// NOLINTEND(bugprone-reserved-identifier)
}
