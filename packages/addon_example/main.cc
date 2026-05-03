// npm run clean; CXX=/usr/lib/llvm-22/bin/clang++ cmake "-DCMAKE_MODULE_PATH=$(npm exec auto_js_cmake_include)" -DCMAKE_BUILD_TYPE=Debug -G Ninja -B build; npm run build
import auto_js;
import isolated_vm;
import std;
import util;
using namespace js;
using namespace isolated_vm;

auto test(const runtime_lock& /*lock*/, std::string message) -> std::int32_t {
	std::print("{}\n", message);
	return 123;
}

isolated_vm::addon module_namespace{
	std::type_identity<std::monostate>{},
	[]() -> auto {
		return std::tuple{
			std::in_place,
			std::pair{util::cw<"test">, js::free_function{util::fn<test>}},
			std::pair{util::cw<"constant">, util::cw<"constant string">},
		};
	}
};
