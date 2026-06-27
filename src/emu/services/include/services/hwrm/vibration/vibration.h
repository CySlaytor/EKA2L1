#pragma once

#include <drivers/hwrm/vibration.h>
#include <services/hwrm/resource.h>

namespace eka2l1 {
    class kernel_system;
}

namespace eka2l1::epoc {
    struct vibration_resource : public resource_interface {
        std::unique_ptr<drivers::hwrm::vibrator> viber_;
        kernel_system *kern_;

        void vibrate_with_default_intensity(service::ipc_context &ctx);
        void vibrate(service::ipc_context &ctx);
        void stop_vibrate(service::ipc_context &ctx);
        void vibrate_cleanup(service::ipc_context &ctx);

        explicit vibration_resource(kernel_system *kern);
        void execute_command(service::ipc_context &ctx) override;
    };
}