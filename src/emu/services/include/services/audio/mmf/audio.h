#pragma once

#include <cstdint>
#include <services/framework.h>

namespace eka2l1 {
    class mmf_dev_server;
    class kernel_system;

    namespace service {
        class session;
    }

    enum mmf_audio_server_opcode {
        mmf_audio_server_set_devsound_info,
        mmf_audio_server_get_dev_session
    };

    class mmf_audio_server_session : public service::typical_session {
    private:
        service::session *dev_session_;

    public:
        explicit mmf_audio_server_session(service::typical_server *serv, kernel::uid client_ss_uid, epoc::version client_version);
        ~mmf_audio_server_session() override;

        void fetch(service::ipc_context *ctx) override;
        void get_dev_session(service::ipc_context *ctx);
    };

    class mmf_audio_server : public service::typical_server {
    private:
        mmf_dev_server *dev_;
        std::uint8_t flags_;

        enum {
            FLAG_INITIALIZED = 1 << 0,
            FLAG_DEVSOUND_INFO_AVAILABLE = 1 << 1
        };

        void init(kernel_system *kern);

    public:
        explicit mmf_audio_server(eka2l1::system *sys, mmf_dev_server *dev);
        void connect(service::ipc_context &context) override;

        mmf_dev_server *get_mmf_dev_server() { return dev_; }
        const bool is_devsound_info_available() const { return flags_ & FLAG_DEVSOUND_INFO_AVAILABLE; }
        kernel_system *get_kernel_system();
    };
}