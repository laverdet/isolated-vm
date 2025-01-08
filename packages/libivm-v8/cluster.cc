module ivm.isolated_v8;
import :cluster;
import :platform;

namespace ivm {

cluster::cluster() :
		platform_handle_{platform::handle::acquire()},
		scheduler_{platform_handle_->scheduler()} {
}

} // namespace ivm
