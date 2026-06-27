#pragma once

namespace eka2l1 {
    enum hwrm_fundamental_op {
        hwrm_fundamental_op_create_vibration_service = 0,
        hwrm_fundamental_op_create_light_service = 1,
        hwrm_fundamental_op_create_power_service = 2,
        hwrm_fundamental_op_create_fm_tx_service = 3
    };

    enum hwrm_light_op {
        hwrm_light_op_on = 1000,
        hwrm_light_op_off = 1001,
        hwrm_light_op_blink = 1002,
        hwrm_light_op_cleanup_lights = 1003,
        hwrm_light_op_reserve_lights = 1004,
        hwrm_light_op_release_lights = 1005,
        hwrm_light_op_supported_targets = 1006,
        hwrm_light_op_set_light_color = 1007
    };

    enum hwrm_vibrate_op {
        hwrm_vibrate_start_with_default_intensity = 2000,
        hwrm_vibrate_start = 2001,
        hwrm_vibrate_stop = 2002,
        hwrm_vibrate_cleanup = 2003
    };
}