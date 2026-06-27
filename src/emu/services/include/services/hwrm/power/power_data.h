#pragma once

namespace eka2l1 {
    class kernel_system;
    namespace service {
        class property;
    }
    using property_ptr = service::property *;

    namespace epoc::hwrm::power {
        struct resource_data {
        private:
            property_ptr charging_status_prop_;
            property_ptr battery_level_prop_;
            property_ptr battery_status_prop_;

        public:
            explicit resource_data(kernel_system *kern);
        };
    }
}