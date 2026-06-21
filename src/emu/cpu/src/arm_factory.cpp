#include <cpu/arm_dynarmic.h>
#include <cpu/arm_factory.h>

namespace eka2l1::arm {
    core_instance create_core(exclusive_monitor *monitor, arm_emulator_type arm_type) {
        // Unconditionally instantiate high-performance Dynarmic core
        return std::make_unique<dynarmic_core>(monitor);
    }

    exclusive_monitor_instance create_exclusive_monitor(arm_emulator_type arm_type, const std::size_t core_count) {
        // Unconditionally instantiate Dynarmic exclusive monitor
        return std::make_unique<dynarmic_exclusive_monitor>(core_count);
    }
}