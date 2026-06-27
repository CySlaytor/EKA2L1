#pragma once

#include <cstdint>
#include <utils/sec.h>

namespace eka2l1::epoc::fs {
    static epoc::security_policy private_comp_access_policy({ epoc::cap_all_files });
    static epoc::security_policy sys_read_only_access_policy({ epoc::cap_all_files });
    static epoc::security_policy resource_read_only_access_policy{};
    static epoc::security_policy sys_resource_modify_access_policy({ epoc::cap_tcb });
}