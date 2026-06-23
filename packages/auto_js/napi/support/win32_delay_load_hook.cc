#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <delayimp.h>
import std;

const auto __pfnDliNotifyHook2 = PfnDliHook{[](unsigned event, DelayLoadInfo* info) -> FARPROC {
	if (event == dliNotePreLoadLibrary && std::string_view{info->szDll} == "node.exe") {
		return reinterpret_cast<FARPROC>(GetModuleHandle(nullptr));
	} else {
		return nullptr;
	}
}};
