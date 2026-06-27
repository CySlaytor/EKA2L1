#include <common/log.h>
#include <kernel/kernel.h>
#include <kernel/property.h>
#include <services/context.h>
#include <services/hwrm/def.h>
#include <services/hwrm/light/light_data.h>

namespace eka2l1::epoc::hwrm::light {
    bool resource_data::initialise_components(kernel_system *kern) {
        infos_prop_ = kern->create<service::property>();

        if (!infos_prop_) {
            LOG_ERROR(SERVICE_HWRM, "Failed to create light service's status property! Abort.");
            return false;
        }

        infos_prop_->first = eka2l1::epoc::hwrm::SERVICE_UID;
        infos_prop_->second = eka2l1::epoc::hwrm::light::LIGHT_STATUS_PROP_KEY;

        infos_prop_->define(service::property_type::bin_data, MAXIMUM_LIGHT * sizeof(target_info));

        for (std::size_t i = 0; i < infos_.size(); i++) {
            infos_[i].target_ = static_cast<std::uint32_t>(1 << i);
            infos_[i].status_ = epoc::hwrm::light::light_status_off;
        }

        if (!infos_prop_->set(reinterpret_cast<std::uint8_t *>(&infos_[0]), MAXIMUM_LIGHT * sizeof(target_info))) {
            LOG_ERROR(SERVICE_HWRM, "Failed to publish default value of light infos to created property. Abort.");
            return false;
        }

        return true;
    }

    resource_data::resource_data(kernel_system *sys) {
        if (!initialise_components(sys)) {
            LOG_ERROR(SERVICE_HWRM, "Unable to initialise light resource data!");
        }
    }
}