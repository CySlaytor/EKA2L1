#pragma once

#include <services/etel/common.h>
#include <services/etel/line.h>

#include <vector>

namespace eka2l1 {
    struct etel_line;

    struct subscriber_id_info_v1 {
        std::int32_t ver_;
        epoc::buf_static<char16_t, 15> the_id_;

        explicit subscriber_id_info_v1()
            : ver_(1) {
        }
    };

    struct etel_phone : public etel_entity {
        epoc::etel_phone_status status_;
        epoc::etel_phone_info info_;
        epoc::etel_phone_network_info network_info_;

        std::vector<etel_line *> lines_;

    public:
        explicit etel_phone(const epoc::etel_phone_info &info);
        ~etel_phone() override;

        bool init();

        epoc::etel_entry_type type() const override {
            return epoc::etel_entry_phone;
        }
    };
}