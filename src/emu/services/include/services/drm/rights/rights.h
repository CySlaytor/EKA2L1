#pragma once

#include <kernel/server.h>
#include <services/drm/rights/db.h>
#include <services/framework.h>

#include <string>

namespace eka2l1 {
    enum rights_opcode {
        rights_opcode_none,
        rights_opcode_get_entry_list = 3,
    };

    enum rights_crediental_check_status {
        rights_crediental_not_checked,
        rights_crediental_checked_and_allowed,
        rights_crediental_checked_and_denied
    };

    static constexpr std::uint32_t ERROR_CA_NO_RIGHTS = -17452;
    static constexpr const char *RIGHTS_SERVER_NAME = "!RightsServer";

    class rights_server : public service::typical_server {
    private:
        std::unique_ptr<epoc::drm::rights_database> database_;

        void initialize();
        void startup_imports();

    public:
        explicit rights_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;

        epoc::drm::rights_database &database() {
            return *database_;
        }
    };

    struct rights_client_session : public service::typical_session {
    private:
        std::string current_key_;
        rights_crediental_check_status check_status_;

    public:
        explicit rights_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void get_entry_list(service::ipc_context *ctx);
    };
}