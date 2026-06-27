#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include <services/hwrm/light/light_def.h>

namespace eka2l1 {
    namespace service {
        class property;
    }
    using property_ptr = service::property *;
    class kernel_system;

    namespace epoc::hwrm::light {
        struct target_info {
            std::uint32_t target_;
            std::uint32_t status_;
        };

        struct resource_data {
        private:
            std::array<target_info, MAXIMUM_LIGHT> infos_;
            property_ptr infos_prop_;

            bool initialise_components(kernel_system *kern);

        public:
            explicit resource_data(kernel_system *kern);
        };
    }
}