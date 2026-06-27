#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace eka2l1::config {
    struct state;
}

namespace eka2l1::epoc::bt {
    enum midman_type {
        MIDMAN_PHYSICAL_BT = 0,
        MIDMAN_INET_BT = 1
    };

    class midman {
    public:
        static constexpr std::size_t MAX_PORT = 60;

    protected:
        std::u16string local_name_;
        void *native_handle_;

    public:
        explicit midman();
        virtual ~midman() = default;

        std::u16string device_name() const {
            return local_name_;
        }

        void device_name(const std::u16string &name) {
            local_name_ = name;
        }

        virtual midman_type type() const = 0;
    };

    std::unique_ptr<midman> make_bluetooth_midman(const eka2l1::config::state &conf, const std::uint32_t reserved_stack_type = 0);
}