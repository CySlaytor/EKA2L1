#pragma once

#include <services/notifier/plugin.h>
#include <vector>

namespace eka2l1 {
    class kernel_system;

    namespace epoc::notifier {
        void add_builtin_plugins(kernel_system *kern, std::vector<plugin_instance> &plugins);
    }
};