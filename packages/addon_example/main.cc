import auto_js;
import isolated_vm;
import std;
using namespace js;
using namespace isolated_vm;

isolated_vm::addon module_namespace{
	std::type_identity<std::monostate>{},
	[](environment& env) -> void {
		auto local = js::transfer_in<value_of<number_tag>>(123, env);
		auto native = js::transfer_out<double>(local, env);
		std::print("hello world {}\n", native);
	}
};
