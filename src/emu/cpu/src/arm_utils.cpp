#include <cstdint>

#include <common/algorithm.h>
#include <common/log.h>
#include <cpu/arm_utils.h>

namespace eka2l1::arm {
    void dump_context(const core::thread_context &uni) {
        LOG_TRACE(CPU, "CPU context: ");
        LOG_TRACE(CPU, "pc: 0x{:x}", uni.get_pc());
        LOG_TRACE(CPU, "lr: 0x{:x}", uni.get_lr());
        LOG_TRACE(CPU, "sp: 0x{:x}", uni.get_sp());
        LOG_TRACE(CPU, "cpsr: 0x{:x}", uni.cpsr);

        for (std::size_t i = 0; i < uni.cpu_registers.size(); i++) {
            LOG_TRACE(CPU, "r{}: 0x{:x}", i, uni.cpu_registers[i]);
        }
    }

    const char *arm_emulator_type_to_string(const arm_emulator_type type) {
        return dynarmic_jit_backend_formal_name;
    }

    arm_emulator_type string_to_arm_emulator_type(const std::string &name) {
        return arm_emulator_type::dynarmic;
    }
}