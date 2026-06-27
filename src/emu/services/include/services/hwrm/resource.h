#pragma once

namespace eka2l1::service {
    struct ipc_context;
}

namespace eka2l1::epoc {
    struct resource_interface {
        virtual ~resource_interface() {}
        virtual void execute_command(service::ipc_context &ctx) = 0;
    };
}