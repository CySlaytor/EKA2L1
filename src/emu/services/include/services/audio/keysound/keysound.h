#pragma once

#include <services/audio/keysound/context.h>
#include <services/framework.h>
#include <vector>

namespace eka2l1 {
    class keysound_session : public service::typical_session {
    private:
        service::uid app_uid_;
        std::vector<epoc::keysound::context> contexts_;

        struct parser_state {
            std::uint32_t frames_;
            std::uint32_t target_freq_;
            std::uint32_t frequency_;
            std::uint32_t ms_;
            std::uint32_t duration_unit_;
            std::size_t parser_pos_;
            epoc::keysound::sound_info sound_;

            explicit parser_state();
            void set(epoc::keysound::sound_info &new_sound);
        } state_;

    public:
        explicit keysound_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_version)
            : service::typical_session(svr, client_ss_uid, client_version) {}
        ~keysound_session() override {}

        void fetch(service::ipc_context *ctx) override;
        void init(service::ipc_context *ctx);
        void push_context(service::ipc_context *ctx);
        void play_key(service::ipc_context *ctx);
        void add_sids(service::ipc_context *ctx);
        void bring_to_foreground(service::ipc_context *ctx);
    };

    class keysound_server : public service::typical_server {
        bool inited_;
        std::vector<epoc::keysound::sound_info> sounds_;

    public:
        explicit keysound_server(system *sys);
        void connect(service::ipc_context &context) override;

        void add_sound(epoc::keysound::sound_info &info) {
            sounds_.push_back(std::move(info));
        }

        bool initialized() const { return inited_; }
        void initialized(const bool is_it) { inited_ = is_it; }
    };
}