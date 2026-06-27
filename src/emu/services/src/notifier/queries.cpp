#include <algorithm>
#include <services/notifier/queries.h>

namespace eka2l1::epoc::notifier {
    void add_builtin_plugins(kernel_system *kern, std::vector<plugin_instance> &plugins) {
        // Builtin plugins have been pruned to reduce bloat
    }
};