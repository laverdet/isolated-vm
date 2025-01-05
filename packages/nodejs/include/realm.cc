export module ivm.node:realm;
import ivm.isolated_v8;
import ivm.utility;

namespace ivm {

export class hosted_realm : util::non_copyable {
	public:
		explicit hosted_realm(ivm::realm realm);

	private:
		ivm::realm realm_;
};

} // namespace ivm
