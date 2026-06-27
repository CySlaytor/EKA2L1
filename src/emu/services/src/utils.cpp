#include <kernel/kernel.h>
#include <services/context.h>
#include <services/utils.h>

namespace eka2l1::service {
    std::optional<epoc::version> get_server_version(kernel_system *kern, ipc_context *ctx) {
        epoc::version ver;

        if (kern->is_ipc_old()) {
            std::optional<std::uint32_t> ver_package = ctx->get_argument_data_from_descriptor<std::uint32_t>(1);
            if (!ver_package) {
                return std::nullopt;
            }

            ver.u32 = ver_package.value();
        } else {
            ver.u32 = ctx->get_argument_value<std::uint32_t>(0).value();
        }

        return ver;
    }
}