export module ivm.node:realm;
import ivm.isolated_v8;
import ivm.utility;

namespace ivm {

export class hosted_realm : util::non_copyable {
	public:
		explicit hosted_realm(isolated_v8::realm realm);

	private:
		isolated_v8::realm realm_;
};

} // namespace ivm
