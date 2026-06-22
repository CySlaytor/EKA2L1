#pragma once

#include <common/log.h> // <--- Injected Logging Macro
#include <common/types.h>

#include <bridge/arg_layout.h>
#include <bridge/bridge_types.h>
#include <bridge/layout_args.h>
#include <bridge/read_arg.h>
#include <bridge/return_val.h>
#include <bridge/write_arg.h>

#include <cstdint>
#include <functional>
#include <type_traits>

namespace eka2l1 {
    namespace hle {

        template <typename T, typename ret, typename... args, size_t... indices>
        std::enable_if_t<!std::is_same_v<ret, void>, void> call(ret (*export_fn)(T *, args...), const args_layout<args...> &layout, std::index_sequence<indices...>, arm::core *cpu, kernel::process *pr, T *data) {
            TRACK_CLASS_COVERAGE(); // <--- Tracks all Non-Void bridged API invocations
            const ret result = (*export_fn)(data, read<args, indices, args...>(cpu, layout, pr)...);
            write_return_value(cpu, result);
        }

        template <typename T, typename... args, size_t... indices>
        void call(void (*export_fn)(T *, args...), const args_layout<args...> &layout, std::index_sequence<indices...>, arm::core *cpu, kernel::process *pr, T *data) {
            TRACK_CLASS_COVERAGE(); // <--- Tracks all Void bridged API invocations
            (*export_fn)(data, read<args, indices, args...>(cpu, layout, pr)...);
        }

        template <typename T, typename ret, typename... args>
        auto bridge(ret (*export_fn)(T *, args...)) {
            constexpr args_layout<args...> layouts = lay_out<typename bridge_type<args>::arm_type...>();

            return [export_fn, layouts](T *data, kernel::process *pr, arm::core *cpu) {
                using indices = std::index_sequence_for<args...>;
                call(export_fn, layouts, indices(), cpu, pr, data);
            };
        }
    }
}