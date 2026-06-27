#pragma once

#include <kernel/server.h>
#include <services/context.h>
#include <services/framework.h>
#include <services/ui/cap/coestorage.h>
#include <services/ui/cap/eiksrv.h>
#include <services/ui/cap/sgc.h>
#include <services/window/window.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace eka2l1 {
    class window_server;

    namespace epoc {
        struct sgc_params {
            std::int32_t window_group_id;
            std::uint32_t bit_flags;
            std::int32_t sp_layout;
            std::int32_t sp_flag;
            std::int32_t app_screen_mode;
        };

        struct window_group;
    }

    enum oom_ui_app_op {
        akns_blank_screen = 71,
        akns_unblank_screen = 72,
        akns_update_key_block_mode = 74,
        akn_eik_app_ui_set_sgc_params = 59,
        akn_eik_app_ui_layout_config_size = 66,
        akn_eik_app_ui_get_layout_config = 67,
        akn_eik_app_ui_move_app_in_z_order = 68
    };

    enum class akn_softkey_loc {
        right,
        left,
        bottom
    };

    struct akn_screen_mode_info {
        std::int32_t mode_num;
        epoc::pixel_twips_and_rot info;
        akn_softkey_loc loc;
        std::uint32_t screen_style_hash;
        epoc::display_mode dmode;
    };

    struct akn_hardware_info {
        std::int32_t state_num;
        std::int32_t key_mode;
        std::int32_t screen_mode;
        std::int32_t alt_screen_mode;
    };

    struct akn_layout_config {
        std::int32_t num_screen_mode;
        eka2l1::ptr<akn_screen_mode_info> screen_modes;
        std::int32_t num_hardware_mode;
        eka2l1::ptr<akn_hardware_info> hardware_infos;
    };

    struct akn_running_app_info {
        std::u16string app_name_;
        std::uint32_t app_uid_;
        int screen_number_;
        kernel::process *associated_;

        enum {
            FLAG_CURRENTLY_PLAY = 0x100
        };

        std::uint16_t flags_;
    };

    std::optional<akn_running_app_info> get_akn_app_info_from_window_group(epoc::window_group *group);

    class oom_ui_app_session : public service::typical_session {
        std::int32_t blank_count;
        bool old_layout;

    public:
        explicit oom_ui_app_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_version, const bool is_old_layout = false);
        void blank_screen(service::ipc_context *ctx);
        void unblank_screen(service::ipc_context *ctx);
        void fetch(service::ipc_context *ctx) override;
    };

    static const char *OOM_APP_UI_SERVER_NAME = "101fdfae_10207218_AppServer";

    class oom_ui_app_server : public service::typical_server {
        friend class oom_ui_app_session;

        void get_layout_config_size(service::ipc_context &ctx);
        void get_layout_config(service::ipc_context &ctx);

        std::string layout_buf;

        std::unique_ptr<epoc::cap::sgc_server> sgc;
        std::unique_ptr<epoc::cap::eik_server> eik;
        std::unique_ptr<epoc::coe_data_storage> coe_storage;

        window_server *winsrv{ nullptr };
        std::mutex lock;

        void connect(service::ipc_context &ctx) override;

    protected:
        std::string get_layout_buf();
        void set_sgc_params(service::ipc_context &ctx, const bool old_layout);
        void update_key_block_mode(service::ipc_context &ctx);

    public:
        explicit oom_ui_app_server(eka2l1::system *sys);
        void init(kernel_system *kern, io_system *io, device_manager *mngr);
        epoc::cap::eik_server *get_eik_server() { return eik.get(); }
        epoc::cap::sgc_server *get_sgc_server();
    };
}