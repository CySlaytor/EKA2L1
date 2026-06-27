#include <common/algorithm.h>
#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/ini.h>
#include <common/log.h>
#include <common/pystr.h>

#include <services/centralrepo/centralrepo.h>
#include <services/centralrepo/cre.h>
#include <services/context.h>
#include <system/devices.h>
#include <system/epoc.h>

#include <utils/err.h>
#include <vfs/vfs.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace eka2l1 {

    central_repo_server::central_repo_server(eka2l1::system *sys)
        : service::server(sys->get_kernel_system(), sys, nullptr, CENTRAL_REPO_SERVER_NAME, true)
        , id_counter(0) {
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_init, "CenRep::Init");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_create_int, "CenRep::CreateInt");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_create_real, "CenRep::CreateReal");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_create_string, "CenRep::CreateString");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_close, "CenRep::Close");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_reset, "CenRep::Reset");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_set_int, "CenRep::SetInt");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_set_string, "CenRep::SetStr");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_set_real, "CenRep::SetStr");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_notify_req, "CenRep::NofReq");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_group_nof_req, "CenRep::GroupNofReq");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_get_int, "CenRep::GetInt");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_get_real, "CenRep::GetReal");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_get_string, "CenRep::GetString");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_notify_req_check, "CenRep::NofReqCheck");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_find_eq_int, "CenRep::FindEqInt");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_find_neq_int, "CenRep::FindNeqInt");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_find_eq_string, "CenRep::FindEqString");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_find, "CenRep::Find");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_get_find_res, "CenRep::GetFindResult");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_notify_cancel, "CenRep::NofCancel");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_group_nof_cancel, "CenRep::GroupNofCancel");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_notify_cancel_all, "CenRep::NofCancelAll");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_transaction_start, "CenRep::TransactionStart");
        REGISTER_IPC(central_repo_server, redirect_msg_to_session, cen_rep_transaction_cancel, "CenRep::TransactionCancel");
    }

    void central_repo_client_session::init(service::ipc_context *ctx) {
        const std::uint32_t repo_uid = *ctx->get_argument_value<std::uint32_t>(0);
        device_manager *mngr = ctx->sys->get_device_manager();
        eka2l1::central_repo *repo = server->load_repo_with_lookup(ctx->sys->get_io_system(), mngr, repo_uid);

        if (!repo) {
            LOG_TRACE(SERVICE_CENREP, "Repository not found with UID 0x{:X}", repo_uid);
            ctx->complete(epoc::error_not_found);
            return;
        }

        central_repo_client_subsession clisubsession;
        clisubsession.attach_repo = repo;
        clisubsession.server = server;

        auto res = client_subsessions.emplace(++idcounter, std::move(clisubsession));

        if (!res.second) {
            ctx->complete(epoc::error_no_memory);
            return;
        }

        repo->attached.push_back(&res.first->second);

        ctx->write_data_to_descriptor_argument<std::uint32_t>(3, idcounter);
        ctx->complete(epoc::error_none);
    }

    void central_repo_server::rescan_drives(eka2l1::io_system *io) {
        for (drive_number d = drive_a; d <= drive_z; d = static_cast<drive_number>(static_cast<int>(d) + 1)) {
            std::optional<eka2l1::drive> drv = io->get_drive_entry(d);

            if (!drv) {
                continue;
            }

            if (drv->media_type == drive_media::rom) {
                rom_drv = d;
            }

            if (static_cast<bool>(drv->attribute & io_attrib_internal) && !static_cast<bool>(drv->attribute & io_attrib_write_protected)) {
                avail_drives.push_back(d);
            }
        }
    }

    void central_repo_server::redirect_msg_to_session(service::ipc_context &ctx) {
        const kernel::uid session_uid = ctx.msg->msg_session->unique_id();
        auto session_ite = client_sessions.find(session_uid);

        if (session_ite == client_sessions.end()) {
            LOG_ERROR(SERVICE_CENREP, "Session ID passed not found 0x{:X}", session_uid);
            ctx.complete(epoc::error_argument);

            return;
        }

        session_ite->second.handle_message(&ctx);
    }

    int central_repo_server::load_repo_adv(eka2l1::io_system *io, device_manager *mngr, central_repo *repo, const std::uint32_t key,
        bool scan_org_only) {
        bool is_first_repo = first_repo;
        first_repo ? (first_repo = false) : 0;

        if (is_first_repo) {
            rescan_drives(io);
        }

        LOG_TRACE(SERVICE_CENREP, "Try to open repo 0x{:X}", key);

        std::u16string keystr = common::utf8_to_ucs2(common::to_string(key, std::hex));

        std::u16string repocre = keystr + u".CRE";
        std::u16string repoini = keystr + u".TXT";

        const std::u16string private_dir_persists = u":\\Private\\10202be9\\";
        const std::u16string firmcode = common::utf8_to_ucs2(common::lowercase_string(mngr->get_current()->firmware_code));
        const std::u16string private_dir_persists_glob = private_dir_persists + u"persists\\";
        const std::u16string private_dir_persists_separate_firm = private_dir_persists_glob + firmcode + u"\\";

        avail_drives.push_back(rom_drv);

        for (auto &drv : avail_drives) {
            std::u16string repo_dir{ drive_to_char16(drv) };

            std::vector<std::u16string> repo_folder_to_searches;

            if (drv != rom_drv) {
                repo_folder_to_searches.push_back(repo_dir + private_dir_persists_separate_firm);
                repo_folder_to_searches.push_back(repo_dir + private_dir_persists_glob);
            }

            repo_folder_to_searches.push_back(repo_dir + private_dir_persists);

            for (const std::u16string &repo_folder : repo_folder_to_searches) {
                if (is_first_repo && !io->exist(repo_folder)) {
                    io->create_directories(repo_folder);
                } else {
                    std::u16string repo_path = repo_folder + repocre;

                    if (io->exist(repo_path)) {
                        symfile repofile = io->open_file(repo_path, READ_MODE | BIN_MODE);

                        if (!repofile) {
                            LOG_ERROR(SERVICE_CENREP, "Found repo but open failed: {}", common::ucs2_to_utf8(repo_path));
                            continue;
                        }

                        std::vector<std::uint8_t> buf;
                        buf.resize(repofile->size());

                        repofile->read_file(&buf[0], 1, static_cast<std::uint32_t>(buf.size()));
                        repofile->close();

                        common::chunkyseri seri(&buf[0], buf.size(), common::SERI_MODE_READ);

                        if (int err = do_state_for_cre(seri, *repo)) {
                            LOG_ERROR(SERVICE_CENREP, "Loading CRE file failed with code: 0x{:X}, repo 0x{:X}", err, key);
                            continue;
                        }

                        repo->reside_place = avail_drives[0];
                        repo->access_count = 1;

                        avail_drives.pop_back();
                        return 0;
                    }
                }
            }
        }

        avail_drives.pop_back();
        return -1;
    }

    eka2l1::central_repo *central_repo_server::load_repo(eka2l1::io_system *io, device_manager *mngr, const std::uint32_t key) {
        eka2l1::central_repo repo;
        if (load_repo_adv(io, mngr, &repo, key, false) != 0) {
            return nullptr;
        }

        repos.emplace(key, std::make_unique<eka2l1::central_repo>(repo));
        return repos[key].get();
    }

    eka2l1::central_repo *central_repo_server::load_repo_with_lookup(eka2l1::io_system *io, device_manager *mngr, const std::uint32_t key) {
        auto result = repos.find(key);

        if (result != repos.end()) {
            result->second->access_count++;
            return result->second.get();
        }

        return load_repo(io, mngr, key);
    }

    void central_repo_client_session::handle_message(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case cen_rep_init: {
            init(ctx);
            break;
        }

        case cen_rep_close: {
            close(ctx);
            break;
        }

        default: {
            const std::uint32_t subsession_uid = *ctx->get_argument_value<std::uint32_t>(3);
            auto subsession_ite = client_subsessions.find(subsession_uid);

            if (subsession_ite == client_subsessions.end()) {
                LOG_ERROR(SERVICE_CENREP, "Subsession ID passed not found 0x{:X}", subsession_uid);
                ctx->complete(epoc::error_argument);
                return;
            }

            subsession_ite->second.handle_message(ctx);
            break;
        }
        }
    }

    int central_repo_client_session::closerep(io_system *io, device_manager *mngr, const std::uint32_t repo_id, decltype(client_subsessions)::iterator repo_subsession_ite) {
        auto &repo_subsession = repo_subsession_ite->second;

        if (repo_id != 0 && repo_subsession.attach_repo->uid != repo_id) {
            LOG_CRITICAL(SERVICE_CENREP, "Fail safe check: REPO id != provided id");
            return -2;
        }

        repo_subsession.write_changes(io, mngr);
        LOG_TRACE(SERVICE_CENREP, "Repo 0x{:X}: changes saved", repo_subsession.attach_repo->uid);

        auto &all_attached = repo_subsession.attach_repo->attached;
        auto attach_this_ite = std::find(all_attached.begin(), all_attached.end(),
            &repo_subsession);

        if (attach_this_ite != all_attached.end()) {
            all_attached.erase(attach_this_ite);
        }

        if (repo_subsession.attach_repo->access_count > 0) {
            repo_subsession.attach_repo->access_count--;
        } else {
            LOG_ERROR(SERVICE_CENREP, "Repo 0x{:X} has access count to be negative!", repo_subsession.attach_repo->uid);
        }

        return 0;
    }

    int central_repo_client_session::closerep(io_system *io, device_manager *mngr, const std::uint32_t repo_id, const std::uint32_t id) {
        auto repo_subsession_ite = client_subsessions.find(id);

        if (repo_subsession_ite == client_subsessions.end()) {
            return -1;
        }

        const int result = closerep(io, mngr, repo_id, repo_subsession_ite);
        if (result == 0) {
            client_subsessions.erase(repo_subsession_ite);
        }

        return result;
    }

    void central_repo_client_session::close(service::ipc_context *ctx) {
        device_manager *mngr = ctx->sys->get_device_manager();
        const int err = closerep(ctx->sys->get_io_system(), mngr, 0, *ctx->get_argument_value<std::uint32_t>(3));

        switch (err) {
        case 0: {
            ctx->complete(epoc::error_none);
            break;
        }

        case -1: {
            ctx->complete(epoc::error_not_found);
            break;
        }

        case -2: {
            ctx->complete(epoc::error_argument);
            break;
        }

        default: {
            LOG_ERROR(SERVICE_CENREP, "Unknown return error from closerep {}", err);
            break;
        }
        }
    }

    void central_repo_server::disconnect(service::ipc_context &ctx) {
        const kernel::uid ss_id = ctx.msg->msg_session->unique_id();
        auto ss_ite = client_sessions.find(ss_id);

        io_system *io = ctx.sys->get_io_system();
        device_manager *mngr = ctx.sys->get_device_manager();

        if (ss_ite != client_sessions.end()) {
            central_repo_client_session &ss = ss_ite->second;

            for (auto ite = ss.client_subsessions.begin(); ite != ss.client_subsessions.end(); ite++) {
                ss.closerep(io, mngr, 0, ite);
            }

            ss.client_subsessions.clear();
            client_sessions.erase(ss_ite);
        }

        ctx.complete(epoc::error_none);
    }

    void central_repo_server::connect(service::ipc_context &ctx) {
        central_repo_client_session session;
        session.server = this;

        const kernel::uid id = ctx.msg->msg_session->unique_id();

        client_sessions.insert(std::make_pair(static_cast<const std::uint32_t>(id), std::move(session)));

        ctx.complete(epoc::error_none);
    }
}