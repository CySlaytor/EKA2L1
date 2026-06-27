#pragma once

#include <services/msv/common.h>
#include <services/msv/operations/base.h>

#include <cstdint>
#include <memory>

namespace eka2l1::epoc::msv {
    class operation_factory {
    private:
        msv_id mtm_uid_;

    public:
        explicit operation_factory(const msv_id id)
            : mtm_uid_(id) {
        }

        virtual std::shared_ptr<operation> create_operation(const msv_id operation_id, const operation_buffer &buffer,
            epoc::notify_info complete_info, const std::uint32_t command)
            = 0;

        const msv_id mtm_uid() const {
            return mtm_uid_;
        }
    };
}