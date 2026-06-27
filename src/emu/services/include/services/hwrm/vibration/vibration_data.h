#pragma once

#include <services/hwrm/vibration/vibration_def.h>

namespace eka2l1 {
    namespace service {
        class property;
    }

    struct central_repo;
    class io_system;
    class device_manager;
    using property_ptr = service::property *;
    class kernel_system;

    namespace epoc::hwrm::vibration {
        struct resource_data {
        private:
            property_ptr status_prop_;
            central_repo *vibra_control_repo_;

            bool initialise_components(kernel_system *kern, io_system *io, device_manager *mngr);
            bool enable_vibration(io_system *io, device_manager *mngr);

        public:
            explicit resource_data(kernel_system *kern, io_system *io, device_manager *mngr);
        };
    }
}