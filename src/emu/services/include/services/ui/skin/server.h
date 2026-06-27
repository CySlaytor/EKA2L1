#pragma once

#include <mem/ptr.h>
#include <memory>
#include <queue>
#include <services/framework.h>
#include <services/ui/skin/chunk_maintainer.h>
#include <services/ui/skin/common.h>
#include <services/ui/skin/icon_cfg.h>
#include <services/ui/skin/settings.h>
#include <services/ui/skin/skn.h>

namespace eka2l1 {
    class akn_skin_server_session : public service::typical_session {
        eka2l1::ptr<void> client_handler_;
        epoc::notify_info nof_info_;
        std::queue<epoc::akn_skin_server_change_handler_notification> nof_list_;

        std::uint32_t flags{ 0 };

        enum {
            ASS_FLAG_CANCELED = 0x1
        };

        void do_set_notify_handler(service::ipc_context *ctx);
        void do_next_event(service::ipc_context *ctx);

    public:
        explicit akn_skin_server_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_version);
        ~akn_skin_server_session() override {}
        void fetch(service::ipc_context *ctx) override;
    };

    class akn_skin_server : public service::typical_server {
        std::unique_ptr<epoc::akn_ss_settings> settings_;
        std::unique_ptr<epoc::akn_skin_icon_config_map> icon_config_map_;
        std::unique_ptr<epoc::akn_skin_chunk_maintainer> chunk_maintainer_;

        chunk_ptr skin_chunk_;
        fbs_server *fbss;

        void do_initialisation();
        void merge_active_skin(eka2l1::io_system *io);

    public:
        explicit akn_skin_server(eka2l1::system *sys);
        void connect(service::ipc_context &ctx) override;
    };
}