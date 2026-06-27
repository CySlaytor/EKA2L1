#pragma once

#include <services/msv/common.h>
#include <utils/reqsts.h>

#include <cstdint>

namespace eka2l1 {
    class msv_server;

    namespace kernel {
        using uid = std::uint64_t;
    }
}

namespace eka2l1::epoc::msv {
    enum operation_state {
        operation_state_idle,
        operation_state_queued,
        operation_state_pending,
        operation_state_success,
        operation_state_failure
    };

    class operation {
    private:
        msv_id operation_id_;
        operation_buffer progress_;
        operation_state state_;

    protected:
        operation_buffer buffer_;
        epoc::notify_info complete_info_;

        template <typename T>
        T *progress_data() {
            if (progress_.size() < sizeof(T)) {
                progress_.resize(sizeof(T));
            }

            return reinterpret_cast<T *>(progress_.data());
        }

    public:
        explicit operation(const msv_id operation_id, const operation_buffer &buffer, epoc::notify_info complete_info)
            : operation_id_(operation_id)
            , progress_()
            , state_(operation_state_idle)
            , buffer_(buffer)
            , complete_info_(complete_info) {}

        virtual void execute(msv_server *server, const kernel::uid process_uid) = 0;
        virtual void cancel() {}

        virtual std::int32_t system_progress(epoc::msv::system_progress_info &progress) { return 0; }

        const msv_id operation_id() const {
            return operation_id_;
        }

        const operation_buffer &progress_buffer() const {
            return progress_;
        }

        const operation_state state() const {
            return state_;
        }

        void state(const operation_state state) {
            state_ = state;
        }
    };
}