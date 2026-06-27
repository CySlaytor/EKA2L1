#pragma once

#include <optional>
#include <utils/version.h>

namespace eka2l1 {
    class kernel_system;
}

namespace eka2l1::service {
    struct ipc_context;

    std::optional<epoc::version> get_server_version(kernel_system *kern, ipc_context *ctx);
}