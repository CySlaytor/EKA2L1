#include <array>
#include <kernel/kernel.h>
#include <kernel/property.h>
#include <services/hwrm/power/power_def.h>
#include <services/ui/cap/eiksrv.h>
#include <services/window/window.h>

namespace eka2l1::epoc::cap {
    enum {
        AVKON_INTERNAL_UID = 0x10207218,
        STATUS_PANE_SYSTEM_DATA_KEY = 0x10000000
    };

    akn_battery_state::akn_battery_state()
        : strength_(BATTERY_LEVEL_MAX)
        , charging_(0)
        , icon_state_(0) {}
    akn_signal_state::akn_signal_state()
        : strength_(SIGNAL_LEVEL_MAX)
        , icon_state_(GPRS_SIGNAL_ICON) {}
    akn_indicator_state::akn_indicator_state()
        : incall_bubble_flags_(0)
        , incall_bubble_allow_in_usual_(0)
        , incall_bubble_allow_in_idle_(0)
        , incall_bubble_disabled_(0) {
        std::fill(visible_indicators_, visible_indicators_ + MAX_VISIBLE_INDICATORS, 0);
        std::fill(visibile_indicator_states_, visibile_indicator_states_ + MAX_VISIBLE_INDICATORS, 0);
    }
    akn_status_pane_data::akn_status_pane_data()
        : foreground_subscriber_id_(0) {}

    eik_status_pane_maintainer::eik_status_pane_maintainer(kernel_system *kern)
        : prop_(nullptr) {
        prop_ = kern->create<service::property>();
        prop_->define(service::property_type::bin_data, sizeof(akn_status_pane_data));
        prop_->first = AVKON_INTERNAL_UID;
        prop_->second = STATUS_PANE_SYSTEM_DATA_KEY;
        publish_data();
    }

    void eik_status_pane_maintainer::publish_data() {
        prop_->set<akn_status_pane_data>(local_data_);
    }

    eik_server::eik_server(kernel_system *kern)
        : status_pane_maintainer_(kern)
        , flags_(0) {}

    void eik_server::init(kernel_system *kern) {
        winserv_ = reinterpret_cast<window_server *>(kern->get_by_name<service::server>(eka2l1::get_winserv_name_by_epocver(
            kern->get_epoc_version())));
    }

    void eik_server::key_block_mode(const bool is_on) {
        flags_ &= ~FLAG_KEY_BLOCK_MODE;
        if (is_on)
            flags_ |= FLAG_KEY_BLOCK_MODE;
        winserv_->set_key_block_active(is_on);
    }
}