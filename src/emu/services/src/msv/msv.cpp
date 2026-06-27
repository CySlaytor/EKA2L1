#include <services/fs/fs.h>
#include <services/fs/std.h>
#include <services/msv/msv.h>
#include <services/msv/store.h>
#include <services/sms/settings.h>

#include <system/epoc.h>
#include <vfs/vfs.h>

#include <utils/consts.h>
#include <utils/err.h>
#include <vfs/vfs.h>

#include <utils/bafl.h>
#include <utils/sec.h>

#include <common/cvt.h>
#include <common/time.h>

#include <system/devices.h>

#include <common/path.h>

namespace eka2l1 {
    static const std::u16string DEFAULT_MSG_DATA_DIR = u"C:\\private\\1000484b\\Mail2\\";
    static const std::u16string DEFAULT_MSG_DATA_DIR_OLD = u"C:\\System\\Mail\\";

    std::string get_msv_server_name_by_epocver(const epocver ver) {
        if (ver < epocver::epoc81a) {
            return "MsvServer";
        }

        return "!MsvServer";
    }

    msv_server::msv_server(eka2l1::system *sys)
        : service::typical_server(sys, get_msv_server_name_by_epocver((sys->get_symbian_version_use())))
        , reg_(sys->get_io_system())
        , fserver_(nullptr)
        , inited_(false) {
    }

    void msv_server::install_rom_mtm_modules() {
        io_system *io = sys->get_io_system();
        std::u16string DEFAULT_MSG_MTM_FILE_DIR;
        if (sys->get_symbian_version_use() <= epocver::eka2) {
            DEFAULT_MSG_MTM_FILE_DIR = u"!:\\System\\mtm\\*.r*";
        } else {
            DEFAULT_MSG_MTM_FILE_DIR = u"!:\\Resource\\Messaging\\Mtm\\*.r*";
        }

        drive_number drv_target = drive_z;

        for (drive_number drv = drive_z; drv >= drive_a; drv--) {
            auto drv_entry = io->get_drive_entry(drv);

            if (drv_entry && (drv_entry->media_type == drive_media::rom)) {
                drv_target = drv;
                break;
            }
        }

        DEFAULT_MSG_MTM_FILE_DIR[0] = drive_to_char16(drv_target);

        auto mtm_module_dir = io->open_dir(DEFAULT_MSG_MTM_FILE_DIR, {}, io_attrib_include_file);

        if (mtm_module_dir) {
            while (std::optional<entry_info> info = mtm_module_dir->get_next_entry()) {
                const std::u16string nearest = utils::get_nearest_lang_file(io,
                    common::utf8_to_ucs2(info->full_path), sys->get_system_language(), drv_target);

                reg_.install_group(nearest);
            }
        }
    }

    io_system *msv_server::get_io_system() {
        return sys->get_io_system();
    }

    static constexpr std::uint32_t SMS_SETTINGS_STORE_UID = 0x1000996E;

    void msv_server::init_sms_settings() {
        epoc::sms::sms_settings settings;
        epoc::sms::supply_sim_settings(sys, &settings);

        std::vector<epoc::msv::entry *> entries = indexer_->get_entries_by_parent(epoc::msv::MSV_ROOT_ID_VALUE);
        for (std::size_t i = 0; i < entries.size(); i++) {
            if (entries[i]->mtm_uid_ == epoc::msv::MSV_MSG_TYPE_UID) {
                std::optional<std::u16string> path = indexer_->get_entry_data_file(entries[i]);
                if (path.has_value()) {
                    io_system *io = sys->get_io_system();
                    symfile f = io->open_file(path.value(), READ_MODE | BIN_MODE);

                    epoc::msv::store the_store;

                    if (f) {
                        eka2l1::ro_file_stream rstream(f.get());
                        the_store.read(rstream);
                    }

                    epoc::msv::store_buffer &buffer = the_store.buffer_for(SMS_SETTINGS_STORE_UID);
                    common::chunkyseri seri(nullptr, 0, common::SERI_MODE_MEASURE);

                    settings.absorb(seri);

                    buffer.resize(seri.size());
                    seri = common::chunkyseri(buffer.data(), buffer.size(), common::SERI_MODE_WRITE);
                    settings.absorb(seri);

                    if (f) {
                        f->close();
                    }

                    f = io->open_file(path.value(), WRITE_MODE | BIN_MODE);

                    if (f) {
                        eka2l1::wo_file_stream wstream(f.get());
                        the_store.write(wstream);

                        f->close();
                    }
                }
            }
        }
    }

    void msv_server::init() {
        device_manager *mngr = sys->get_device_manager();
        message_folder_ = eka2l1::add_path(kern->is_eka1() ? DEFAULT_MSG_DATA_DIR_OLD : DEFAULT_MSG_DATA_DIR,
            common::utf8_to_ucs2(mngr->get_current()->firmware_code) + u"\\");

        io_system *io = sys->get_io_system();

        if (!io->exist(message_folder_)) {
            io->create_directories(message_folder_);
        }

        if (kern->is_eka1()) {
            reg_.set_list_path(u"C:\\System\\mtm\\" + common::utf8_to_ucs2(mngr->get_current()->firmware_code)
                + u"\\Mtm Registry v2");
        }

        reg_.load_mtm_list();
        install_rom_mtm_modules();

        indexer_ = std::make_unique<epoc::msv::sql_entry_indexer>(io, message_folder_, static_cast<int>(sys->get_system_language()));

        fserver_ = reinterpret_cast<fs_server *>(kern->get_by_name<service::server>(eka2l1::epoc::fs::get_server_name_through_epocver(
            kern->get_epoc_version())));

        init_sms_settings();
        inited_ = true;
    }

    void msv_server::connect(service::ipc_context &context) {
        if (!inited_) {
            init();
        }

        msv_client_session *sess = create_session<msv_client_session>(&context);

        msv_event_data ready;
        ready.arg1_ = 0;
        ready.arg2_ = 0;
        ready.nof_ = epoc::msv::change_notification_type_index_loaded;

        sess->queue(ready);
        context.complete(epoc::error_none);
    }

    void msv_server::queue(msv_event_data &evt) {
        std::vector<msv_event_data> reports;

        if (evt.ids_.size() > MAX_ID_PER_EVENT_DATA_REPORT) {
            for (std::size_t i = 0; i < (evt.ids_.size() + MAX_ID_PER_EVENT_DATA_REPORT - 1) / MAX_ID_PER_EVENT_DATA_REPORT; i++) {
                msv_event_data copy;
                copy.nof_ = evt.nof_;
                copy.arg1_ = evt.arg1_;
                copy.arg2_ = evt.arg2_;

                copy.ids_.insert(copy.ids_.begin(), evt.ids_.begin() + i * MAX_ID_PER_EVENT_DATA_REPORT, (evt.ids_.size() < (i + 1) * MAX_ID_PER_EVENT_DATA_REPORT) ? evt.ids_.end() : evt.ids_.begin() + (i + 1) * MAX_ID_PER_EVENT_DATA_REPORT);

                reports.push_back(copy);
            }
        }

        for (auto &session : sessions) {
            msv_client_session *cli = reinterpret_cast<msv_client_session *>(session.second.get());

            if (!reports.empty()) {
                for (std::size_t i = 0; i < reports.size(); i++) {
                    cli->queue(reports[i]);
                }
            } else {
                cli->queue(evt);
            }
        }
    }

    void msv_server::install_factory(std::unique_ptr<epoc::msv::operation_factory> &factory) {
        factories_.push_back(std::move(factory));
    }

    msv_client_session::msv_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version)
        , flags_(0)
        , nof_sequence_(0) {
    }

    static void pack_change_buffer(epoc::des8 *buf, kernel::process *pr, const msv_event_data &data) {
        std::uint32_t *buffer_raw = reinterpret_cast<std::uint32_t *>(buf->get_pointer_raw(pr));

        if (!buffer_raw) {
            LOG_ERROR(SERVICE_MSV, "Can't get change buffer raw pointer!");
            return;
        }

        const std::uint32_t total_int32 = static_cast<std::uint32_t>(4 + data.ids_.size());

        if (buf->get_max_length(pr) < total_int32 * sizeof(std::uint32_t)) {
            LOG_ERROR(SERVICE_MSV, "Insufficent change buffer size");
            return;
        }

        buffer_raw[0] = static_cast<std::uint32_t>(data.nof_);
        buffer_raw[1] = data.arg1_;
        buffer_raw[2] = data.arg2_;
        buffer_raw[3] = static_cast<std::uint32_t>(data.ids_.size());

        for (std::size_t i = 0; i < data.ids_.size(); i++) {
            buffer_raw[4 + i] = data.ids_[i];
        }

        buf->set_length(pr, total_int32 * sizeof(std::uint32_t));
    }

    bool msv_client_session::listen(epoc::notify_info &info, epoc::des8 *change, epoc::des8 *seq) {
        if (!msv_info_.empty()) {
            return false;
        }

        if (!events_.empty()) {
            msv_event_data evt = std::move(events_.front());
            events_.pop();

            kernel::process *own_pr = info.requester->owning_process();

            nof_sequence_++;
            std::string nof_sequence_data(reinterpret_cast<const char *>(&nof_sequence_), sizeof(std::uint32_t));

            pack_change_buffer(change, own_pr, evt);
            seq->assign(own_pr, nof_sequence_data);

            info.complete(epoc::error_none);
            return true;
        }

        msv_info_ = info;
        change_ = change;
        sequence_ = seq;

        return true;
    }

    void msv_client_session::queue(msv_event_data evt) {
        nof_sequence_++;
        std::string nof_sequence_data(reinterpret_cast<const char *>(&nof_sequence_), sizeof(std::uint32_t));

        if (!msv_info_.empty()) {
            kernel::process *own_pr = msv_info_.requester->owning_process();

            pack_change_buffer(change_, own_pr, evt);
            sequence_->assign(own_pr, nof_sequence_data);

            msv_info_.complete(epoc::error_none);
            return;
        }

        events_.push(std::move(evt));
    }

    void msv_client_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case msv_notify_session_event: {
            notify_session_event(ctx);
            break;
        }

        case msv_cancel_notify_session_event:
            cancel_notify_session_event(ctx);
            break;

        case msv_get_message_directory:
            get_message_directory(ctx);
            break;

        case msv_get_message_drive:
            get_message_drive(ctx);
            break;

        default: {
            LOG_ERROR(SERVICE_MSV, "Unimplemented opcode for MsvServer 0x{:X}", ctx->msg->function);
            ctx->complete(epoc::error_none);
            break;
        }
        }
    }

    void msv_client_session::notify_session_event(service::ipc_context *ctx) {
        kernel::process *own_pr = ctx->msg->own_thr->owning_process();
        epoc::notify_info info(ctx->msg->request_sts, ctx->msg->own_thr);

        epoc::des8 *change = eka2l1::ptr<epoc::des8>(ctx->msg->args.args[0]).get(own_pr);
        epoc::des8 *select = eka2l1::ptr<epoc::des8>(ctx->msg->args.args[1]).get(own_pr);

        if (!listen(info, change, select)) {
            LOG_ERROR(SERVICE_MSV, "Request listen on an already-listening MSV session");
            ctx->complete(epoc::error_in_use);
        }
    }

    void msv_client_session::cancel_notify_session_event(service::ipc_context *ctx) {
        if (!msv_info_.empty()) {
            msv_info_.complete(epoc::error_cancel);
        }

        ctx->complete(epoc::error_none);
    }

    void msv_client_session::get_message_directory(service::ipc_context *ctx) {
        const std::u16string folder = server<msv_server>()->message_folder();

        ctx->write_arg(0, folder);
        ctx->complete(epoc::error_none);
    }

    void msv_client_session::get_message_drive(service::ipc_context *ctx) {
        const std::u16string folder = server<msv_server>()->message_folder();
        const drive_number drv = char16_to_drive(folder[0]);

        ctx->complete(static_cast<int>(drv));
    }
}