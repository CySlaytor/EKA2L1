#pragma once

#include <services/framework.h>
#include <services/hwrm/light/light_data.h>
#include <services/hwrm/power/power_data.h>
#include <services/hwrm/resource.h>
#include <services/hwrm/vibration/vibration_data.h>

#include <memory>

namespace eka2l1 {
    class hwrm_session : public service::typical_session {
        std::unique_ptr<epoc::resource_interface> resource_;

    public:
        explicit hwrm_session(service::typical_server *serv, kernel::uid client_ss_uid, epoc::version client_version);
        void fetch(service::ipc_context *ctx) override;
    };

    class hwrm_server : public service::typical_server {
        std::unique_ptr<epoc::hwrm::light::resource_data> light_data_;
        std::unique_ptr<epoc::hwrm::vibration::resource_data> vibration_data_;
        std::unique_ptr<epoc::hwrm::power::resource_data> power_data_;

        bool init();

    public:
        explicit hwrm_server(system *sys);
        void connect(service::ipc_context &ctx) override;
    };
}