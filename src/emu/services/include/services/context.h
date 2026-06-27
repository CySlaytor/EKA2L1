#pragma once

#include <common/algorithm.h>
#include <common/log.h>
#include <cstring>
#include <kernel/ipc.h>
#include <mem/ptr.h>
#include <memory>
#include <optional>
#include <string>

namespace eka2l1 {
    class system;

    namespace epoc {
        struct security_info;
        struct security_policy;
    }

    namespace service {
        struct ipc_context {
            explicit ipc_context();
            ~ipc_context();

            eka2l1::system *sys;
            ipc_msg_ptr msg;

            bool signaled = false;
            bool auto_deref = true;
            bool accurate_timing = false;

            template <typename T>
            std::optional<T> get_argument_value(const int idx);

            template <typename T>
            std::optional<T> get_argument_data_from_descriptor(const int idx, const bool ignore_size_diff = false) {
                std::uint8_t *data = get_descriptor_argument_ptr(idx);
                if (!data) {
                    return std::nullopt;
                }
                const std::size_t packed_size = get_argument_data_size(idx);
                if ((packed_size != sizeof(T)) && !ignore_size_diff) {
                    LOG_WARN(SERVICE_TRACK, "Getting packed struct with mismatch size ({} vs {}), size to get "
                                            "will be automatically clamped",
                        packed_size, sizeof(T));
                }
                T object;
                std::copy(data, data + common::min(packed_size, sizeof(T)), reinterpret_cast<std::uint8_t *>(&object));
                return std::make_optional<T>(std::move(object));
            }

            void complete(int res);
            std::uint8_t *get_descriptor_argument_ptr(int idx);
            std::size_t get_argument_data_size(int idx);
            std::size_t get_argument_max_data_size(int idx);
            bool set_descriptor_argument_length(const int idx, const std::uint32_t len);
            bool write_arg(int idx, const std::u16string &data);
            bool write_data_to_descriptor_argument(int idx, const uint8_t *data, uint32_t len, int *err_code = nullptr, const bool auto_shrink_to_fit = false);

            template <typename T>
            bool write_data_to_descriptor_argument(int idx, const T &data, int *err_code = nullptr, const bool auto_shrink_to_fit = false) {
                return write_data_to_descriptor_argument(idx, reinterpret_cast<const uint8_t *>(&data), sizeof(T), err_code, auto_shrink_to_fit);
            }

            bool satisfy(epoc::security_policy &policy, epoc::security_info *missing = nullptr);
        };
    }
}