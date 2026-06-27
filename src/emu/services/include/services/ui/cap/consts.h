#pragma once

#include <cstdint>

namespace eka2l1::epoc {
    static constexpr std::uint32_t FEP_FRAMEWORK_REPO_UID = 0x10272618;
    static constexpr std::uint32_t FEP_RESOURCE_ID = 0x008;

    enum fep_framework_repo_key {
        fep_framework_repo_keymask_default_setting = 0x1000,
        fep_framework_repo_keymask_dynamic_setting = 0x2000,

        fep_framework_repo_keymask_fepid = 0x1,
        fep_framework_repo_keymask_on_state = 0x2,
        fep_framework_repo_keymask_on_key_data = 0x4,
        fep_framework_repo_keymask_off_key_data = 0x8,

        fep_framework_repo_key_default_fepid = fep_framework_repo_keymask_default_setting | fep_framework_repo_keymask_fepid,
        fep_framework_repo_key_default_on_state = fep_framework_repo_keymask_default_setting | fep_framework_repo_keymask_on_state,
        fep_framework_repo_key_default_on_key_data = fep_framework_repo_keymask_default_setting | fep_framework_repo_keymask_on_key_data,
        fep_framework_repo_key_default_off_key_data = fep_framework_repo_keymask_default_setting | fep_framework_repo_keymask_off_key_data,

        fep_framework_repo_key_dynamic_fepid = fep_framework_repo_keymask_dynamic_setting | fep_framework_repo_keymask_fepid,
        fep_framework_repo_key_dynamic_on_state = fep_framework_repo_keymask_dynamic_setting | fep_framework_repo_keymask_on_state,
        fep_framework_repo_key_dynamic_on_key_data = fep_framework_repo_keymask_dynamic_setting | fep_framework_repo_keymask_on_key_data,
        fep_framework_repo_key_dynamic_off_key_data = fep_framework_repo_keymask_dynamic_setting | fep_framework_repo_keymask_off_key_data
    };
}