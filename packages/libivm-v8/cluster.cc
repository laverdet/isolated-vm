module ivm.isolated_v8;
import :cluster;
import :platform;

namespace ivm {

cluster::cluster() :
		platform_handle_{platform::handle::acquire()} {
}

} // namespace ivm
