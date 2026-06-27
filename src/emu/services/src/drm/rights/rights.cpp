#include <common/crypt.h>
#include <common/pystr.h>
#include <services/drm/rights/rights.h>
#include <services/fs/fs.h>
#include <system/epoc.h>
#include <utils/err.h>
#include <vfs/vfs.h>

namespace eka2l1 {
    static constexpr const char16_t *RIGHTS_FOLDER_PATH = u"C:\\Private\\101f51f2\\";
    static constexpr const char16_t *RIGHTS_DB_PATH = u"C:\\Private\\101f51f2\\rights.db";
    static constexpr const char16_t *TEMP_FILE_PATH = u"C:\\system\\temp\\";

    rights_server::rights_server(eka2l1::system *sys)
        : service::typical_server(sys, RIGHTS_SERVER_NAME) {
    }

    void rights_server::initialize() {
        io_system *io = sys->get_io_system();
        if (!io) {
            return;
        }

        io->create_directories(RIGHTS_FOLDER_PATH);

        std::optional<std::u16string> path = io->get_raw_path(RIGHTS_DB_PATH);
        if (!path.has_value()) {
            LOG_ERROR(SERVICE_DRMSYS, "No real path to rights DB available!");
            return;
        }

        database_ = std::make_unique<epoc::drm::rights_database>(path.value());
        startup_imports();
    }

    void rights_server::startup_imports() {
        // Stubbed due to external dependencies pruned. Kept body minimal to preserve function signature and log expectations.
        if (database_) {
            database_->flush();
        }
    }

    void rights_server::connect(service::ipc_context &context) {
        if (!database_) {
            initialize();
        }

        create_session<rights_client_session>(&context);
        context.complete(epoc::error_none);
    }

    rights_client_session::rights_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version)
        , check_status_(rights_crediental_not_checked) {
    }

    void rights_client_session::get_entry_list(service::ipc_context *ctx) {
        std::optional<std::string> cid = ctx->get_argument_value<std::string>(1);

        if (!cid.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        kernel_system *kern = ctx->sys->get_kernel_system();
        fs_server *fss = kern->get_by_name<fs_server>(epoc::fs::get_server_name_through_epocver(kern->get_epoc_version()));

        if (!fss) {
            LOG_ERROR(SERVICE_DRMSYS, "Can't establish a connection to Rights server!");
            ctx->complete(epoc::error_general);
            return;
        }

        symfile temp = fss->get_temp_file(TEMP_FILE_PATH);
        if (!temp) {
            LOG_ERROR(SERVICE_DRMSYS, "Can't open a new temp file to write entry list!");
            ctx->complete(epoc::error_general);

            return;
        }

        std::vector<epoc::drm::rights_permission> perms;
        rights_server *rserv = server<rights_server>();

        if (!rserv->database().get_permission_list(cid.value(), perms)) {
            ctx->complete(ERROR_CA_NO_RIGHTS);
            return;
        }

        // Extensively stripped down to remove dependencies while remaining compatible
        std::uint32_t perm_count = static_cast<std::uint32_t>(perms.size());
        temp->write_file(&perm_count, 1, 4);

        ctx->write_arg(0, temp->file_name());
        temp->close();

        ctx->complete(epoc::error_none);
    }

    void rights_client_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case rights_opcode_get_entry_list:
            get_entry_list(ctx);
            break;

        default:
            LOG_ERROR(SERVICE_DRMSYS, "Unimplemented opcode for RightsServer 0x{:X}", ctx->msg->function);
            ctx->complete(epoc::error_none);
        }
    }
}