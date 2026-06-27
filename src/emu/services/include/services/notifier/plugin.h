#pragma once

#include <utils/consts.h>
#include <utils/des.h>
#include <utils/reqsts.h>

#include <cstdint>
#include <memory>

namespace eka2l1 {
    class kernel_system;
}

namespace eka2l1::epoc::notifier {
    class plugin_base {
    protected:
        kernel_system *kern_;

    public:
        explicit plugin_base(kernel_system *kern)
            : kern_(kern) {
        }

        virtual ~plugin_base() {
        }

        virtual epoc::uid unique_id() const = 0;
        virtual void handle(epoc::desc8 *request, epoc::des8 *respone, epoc::notify_info &complete_info) = 0;
        virtual void cancel() = 0;
    };

    using plugin_instance = std::unique_ptr<plugin_base>;
};