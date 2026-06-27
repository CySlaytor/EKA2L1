#include <common/log.h>
#include <kernel/kernel.h>
#include <services/hwrm/power/power_data.h>
#include <services/hwrm/power/power_def.h>

namespace eka2l1::epoc::hwrm::power {
    resource_data::resource_data(kernel_system *kern)
        : charging_status_prop_(nullptr)
        , battery_level_prop_(nullptr)
        , battery_status_prop_(nullptr) {
        charging_status_prop_ = kern->create<service::property>();
        battery_level_prop_ = kern->create<service::property>();
        battery_status_prop_ = kern->create<service::property>();

        if (!charging_status_prop_ || !battery_level_prop_ || !battery_status_prop_) {
            LOG_ERROR(SERVICE_HWRM, "Failed to create power service's properties! Abort.");
            return;
        }

        charging_status_prop_->first = STATE_UID;
        charging_status_prop_->second = CHARGING_STATUS_KEY;

        battery_level_prop_->first = STATE_UID;
        battery_level_prop_->second = BATTERY_LEVEL_KEY;

        battery_status_prop_->first = STATE_UID;
        battery_status_prop_->second = BATTERY_STATUS_KEY;

        charging_status_prop_->define(service::property_type::int_data, sizeof(std::uint32_t));
        battery_level_prop_->define(service::property_type::int_data, sizeof(std::uint32_t));
        battery_status_prop_->define(service::property_type::int_data, sizeof(std::uint32_t));

        charging_status_prop_->set_int(static_cast<int>(CHARGING_STATUS_CHARGING));
        battery_level_prop_->set_int(static_cast<int>(BATTERY_LEVEL_MAX));
        battery_status_prop_->set_int(static_cast<int>(BATTERY_STATUS_OK));
    }
}