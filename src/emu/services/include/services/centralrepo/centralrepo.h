#pragma once

#include <kernel/server.h>
#include <services/centralrepo/repo.h>

#include <atomic>
#include <map>
#include <string>
#include <unordered_map>

#define CENTRAL_REPO_UID_STRING "10202be9"
#define CENTRAL_REPO_SERVER_NAME "!CentralRepository"

namespace eka2l1 {
    class io_system;
    class device_manager;

    class central_repo_server;

    struct central_repo_client_session {
        central_repo_server *server;

        std::uint32_t idcounter{ 0 };
        std::unordered_map<std::uint32_t, central_repo_client_subsession> client_subsessions;

        central_repo_client_session() {}

        void handle_message(service::ipc_context *ctx);

        void init(service::ipc_context *ctx);
        void close(service::ipc_context *ctx);

        int closerep(io_system *io, device_manager *mngr, const std::uint32_t repo_id, const std::uint32_t id);
        int closerep(io_system *io, device_manager *mngr, const std::uint32_t repo_id, decltype(client_subsessions)::iterator clisub);
    };

    class central_repo_server : public service::server {
        friend struct central_repo_client_session;

        std::unordered_map<std::uint32_t, std::unique_ptr<central_repo>> repos;
        std::unordered_map<std::uint64_t, central_repo_client_session> client_sessions;

        drive_number rom_drv;

        std::atomic<std::uint32_t> id_counter;

        std::vector<drive_number> avail_drives;
        std::mutex serv_lock;

        bool first_repo = true;

    protected:
        void rescan_drives(eka2l1::io_system *io);

        int load_repo_adv(eka2l1::io_system *io, device_manager *mngr, central_repo *repo, const std::uint32_t key,
            bool scan_org_only = false);

        eka2l1::central_repo *load_repo(eka2l1::io_system *io, device_manager *mngr, const std::uint32_t key);

    public:
        void redirect_msg_to_session(service::ipc_context &ctx);

        explicit central_repo_server(eka2l1::system *sys);

        eka2l1::central_repo *load_repo_with_lookup(eka2l1::io_system *io, device_manager *mngr, const std::uint32_t key);

        void connect(service::ipc_context &ctx) override;
        void disconnect(service::ipc_context &ctx) override;
    };
}