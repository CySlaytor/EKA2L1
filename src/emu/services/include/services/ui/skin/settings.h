#pragma once

#include <cstdint>
#include <services/ui/skin/common.h>

namespace eka2l1 {
    struct central_repo;
    class central_repo_server;
    class io_system;
    class device_manager;
}

namespace eka2l1::epoc {
    class akn_ss_settings {
        central_repo *avkon_rep_{ nullptr };
        central_repo *skins_rep_{ nullptr };
        central_repo *theme_rep_{ nullptr };

        io_system *io_;
        device_manager *dvcmngr_;

        pid default_skin_pid_{ 0, 0 };
        pid active_skin_pid_{ 0, 0 };
        bool ah_mirroring_active;
        bool highlight_anim_enabled;

    protected:
        bool read_default_skin_id();
        bool read_ah_mirroring_active();
        bool read_active_skin_id();
        bool read_highlight_anim_enabled();
        void set_pid_to_skins_repo(const std::uint32_t key, const epoc::pid pid, const bool uid_only);

    public:
        explicit akn_ss_settings(io_system *io, central_repo_server *svr);
        pid active_skin_pid() const { return active_skin_pid_; }
        pid default_skin_pid() const { return default_skin_pid_; }

        void active_skin_pid(const epoc::pid pid);
        void default_skin_pid(const epoc::pid pid);
    };
}