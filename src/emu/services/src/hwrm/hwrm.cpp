#include <services/hwrm/hwrm.h>
#include <services/hwrm/op.h>
#include <services/hwrm/vibration/vibration.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1 {
    hwrm_session::hwrm_session(service::typical_server *serv, kernel::uid client_ss_uid, epoc::version client_version)
        : service::typical_session(serv, client_ss_uid, client_version) {
    }

    void hwrm_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case hwrm_fundamental_op_create_vibration_service: {
            resource_ = std::make_unique<epoc::vibration_resource>(ctx->sys->get_kernel_system());
            ctx->complete(epoc::error_none);
            break;
        }

        case hwrm_fundamental_op_create_light_service: {
            ctx->complete(epoc::error_none);
            break;
        }

        default: {
            if (ctx->msg->function >= 1000) {
                if (resource_)
                    resource_->execute_command(*ctx);
                else
                    ctx->complete(epoc::error_none);
            } else {
                LOG_ERROR(SERVICE_HWRM, "Unimplemented opcode for HWMR session 0x{:X}", ctx->msg->function);
            }
            break;
        }
        }
    }

    hwrm_server::hwrm_server(system *sys)
        : service::typical_server(sys, "!HWRMServer") {
        power_data_ = std::make_unique<epoc::hwrm::power::resource_data>(kern);
    }

    void hwrm_server::connect(service::ipc_context &ctx) {
        if (!light_data_ || !vibration_data_) {
            if (!init()) {
                LOG_ERROR(SERVICE_HWRM, "Fail to initialise HWRM service shared data!");
            }
        }

        create_session<hwrm_session>(&ctx);
        typical_server::connect(ctx);
    }

    bool hwrm_server::init() {
        kernel_system *kern = sys->get_kernel_system();
        light_data_ = std::make_unique<epoc::hwrm::light::resource_data>(kern);
        vibration_data_ = std::make_unique<epoc::hwrm::vibration::resource_data>(kern, sys->get_io_system(),
            sys->get_device_manager());
        return true;
    }
}