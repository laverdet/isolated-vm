module;
#include <memory>
#include <type_traits>
export module ivm.isolated_v8:platform;
import ivm.utility;
import v8;

namespace ivm {

// Once per process, performs initialization of v8. Process-wide shared state is managed in this
// class.
export class platform : non_moveable, public v8::Platform {
	public:
		platform();
		~platform();

	private:
		// TODO: This is just the default platform, which must be a pointer.
		std::unique_ptr<v8::Platform> v8_platform;
};

platform::platform() :
		v8_platform{v8::platform::NewDefaultPlatform()} {
	v8::V8::InitializeICU();
	v8::V8::InitializePlatform(v8_platform.get());
	v8::V8::Initialize();
}

platform::~platform() {
	v8::V8::Dispose();
	v8::V8::DisposePlatform();
}

} // namespace ivm
