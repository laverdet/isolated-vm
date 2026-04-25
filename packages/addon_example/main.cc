import isolated_vm;
import std;

static auto nothing = []() {
	std::printf("hello world\n");
	return 0;
}();
