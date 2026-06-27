#pragma once

#include <services/msv/common.h>
#include <services/msv/entry.h>
#include <services/msv/factory.h>
#include <services/msv/operations/base.h>
#include <services/msv/registry.h>

#include <kernel/server.h>
#include <services/framework.h>

#include <utils/des.h>
#include <utils/reqsts.h>

#include <memory>
#include <queue>

namespace eka2l1 {
    class io_system;
    class fs_server;

    enum msv_opcode {
        msv_notify_session_event = 0xB,
        msv_cancel_notify_session_event = 0xC,
        msv_get_message_directory = 0x25,
        msv_get_message_drive = 0x33
    };

    struct msv_event_data {
        epoc::msv::change_notification_type nof_;
        std::uint32_t arg1_;
        std::uint32_t arg2_;
        std::vector<std::uint32_t> ids_;
    };

    static constexpr std::uint32_t MAX_ID_PER_EVENT_DATA_REPORT = 15;

    class msv_server : public service::typical_server {
        friend struct msv_client_session;

        std::u16string message_folder_;
        epoc::msv::mtm_registry reg_;

        std::unique_ptr<epoc::msv::entry_indexer> indexer_;
        std::vector<std::unique_ptr<epoc::msv::operation_factory>> factories_;

        fs_server *fserver_;

        bool inited_;

    protected:
        void install_rom_mtm_modules();
        void init();
        void init_sms_settings();

    public:
        explicit msv_server(eka2l1::system *sys);

        const std::u16string message_folder() const {
            return message_folder_;
        }

        epoc::msv::entry_indexer *indexer() {
            return indexer_.get();
        }

        io_system *get_io_system();

        void connect(service::ipc_context &context) override;
        void queue(msv_event_data &evt);

        void install_factory(std::unique_ptr<epoc::msv::operation_factory> &factory);
    };

    struct msv_client_session : public service::typical_session {
        epoc::notify_info msv_info_;
        epoc::des8 *change_;
        epoc::des8 *sequence_;

        std::queue<msv_event_data> events_;
        std::uint32_t nof_sequence_;

        std::uint32_t flags_;

    protected:
        bool listen(epoc::notify_info &info, epoc::des8 *change, epoc::des8 *seq);

    public:
        explicit msv_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);

        void fetch(service::ipc_context *ctx) override;
        void notify_session_event(service::ipc_context *ctx);
        void cancel_notify_session_event(service::ipc_context *ctx);
        void get_message_directory(service::ipc_context *ctx);
        void get_message_drive(service::ipc_context *ctx);

        void queue(msv_event_data evt);
    };
}