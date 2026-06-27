#pragma once

#include <services/etel/common.h>

namespace eka2l1 {
    struct etel_line : public etel_entity {
    public:
        epoc::etel_line_info info_;
        std::uint32_t caps_;
        std::string name_;

    public:
        explicit etel_line(const epoc::etel_line_info &info, const std::string &name,
            const std::uint32_t caps);

        ~etel_line() override;

        epoc::etel_entry_type type() const override {
            return epoc::etel_entry_line;
        }
    };
}