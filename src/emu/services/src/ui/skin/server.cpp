#include <common/cvt.h>
#include <common/log.h>
#include <kernel/kernel.h>
#include <loader/e32img.h>
#include <services/fbs/fbs.h>
#include <services/ui/skin/common.h>
#include <services/ui/skin/server.h>
#include <services/ui/skin/skn.h>
#include <services/ui/skin/utils.h>
#include <system/epoc.h>
#include <utils/err.h>
#include <vfs/vfs.h>

namespace eka2l1 {
    static const epoc::pid DEFAULT_ALWAYS_EXIST_SKIN_PID = epoc::pid(0x101F84B9, 0);

    akn_skin_server_session::akn_skin_server_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_version)
        : service::typical_session(svr, client_ss_uid, client_version) {
    }

    void akn_skin_server_session::do_set_notify_handler(service::ipc_context *ctx) {
        client_handler_ = *ctx->get_argument_value<std::uint32_t>(0);
        ctx->complete(epoc::error_none);
    }

    void akn_skin_server_session::do_next_event(service::ipc_context *ctx) {
        if (flags & ASS_FLAG_CANCELED) {
            while (!nof_list_.empty())
                nof_list_.pop();
            ctx->complete(epoc::error_cancel);
            nof_info_.complete(epoc::error_cancel);
            return;
        }

        if (nof_list_.size() > 0) {
            const epoc::akn_skin_server_change_handler_notification nof_code = std::move(nof_list_.front());
            nof_list_.pop();
            ctx->complete(static_cast<int>(nof_code));
        } else {
            nof_info_.requester = ctx->msg->own_thr;
            nof_info_.sts = ctx->msg->request_sts;
        }
    }

    void akn_skin_server_session::fetch(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case 5: // epoc::akn_skin_server_set_notify_handler
            do_set_notify_handler(ctx);
            break;
        case 6: // epoc::akn_skin_server_next_event
            do_next_event(ctx);
            break;
        default:
            LOG_ERROR(SERVICE_UI, "Unimplemented skin server opcode: {}", ctx->msg->function);
            ctx->complete(epoc::error_none);
            break;
        }
    }

    akn_skin_server::akn_skin_server(eka2l1::system *sys)
        : service::typical_server(sys, "!AknSkinServer")
        , settings_(nullptr) {
    }

    void akn_skin_server::connect(service::ipc_context &ctx) {
        if (!settings_) {
            do_initialisation();
        }
        create_session<akn_skin_server_session>(&ctx);
        ctx.complete(epoc::error_none);
    }

    void akn_skin_server::merge_active_skin(eka2l1::io_system *io) {
        epoc::pid skin_pid = settings_->active_skin_pid();
        if (skin_pid.first == 0) {
            epoc::pid default_pid = settings_->default_skin_pid();
            if (default_pid.first == 0) {
                std::optional<epoc::pid> picked = epoc::pick_first_skin(io);
                if (!picked) {
                    LOG_ERROR(SERVICE_UI, "Unable to pick a skin as default!");
                    return;
                }
                settings_->active_skin_pid(picked.value());
                settings_->default_skin_pid(picked.value());
                skin_pid = picked.value();
            } else {
                settings_->active_skin_pid(default_pid);
                skin_pid = default_pid;
            }
        }

        std::optional<std::u16string> skin_path = epoc::find_skin_file(io, skin_pid);
        if (!skin_path.has_value()) {
            skin_pid = DEFAULT_ALWAYS_EXIST_SKIN_PID;
            skin_path = epoc::find_skin_file(io, skin_pid);
            if (!skin_path.has_value()) {
                LOG_ERROR(SERVICE_UI, "Unable to find active skin file!");
                return;
            }
        }

        std::optional<std::u16string> resource_path = epoc::get_resource_path_of_skin(io, skin_pid);
        symfile skin_file_obj = io->open_file(skin_path.value(), READ_MODE | BIN_MODE);
        eka2l1::ro_file_stream skin_file_stream(skin_file_obj.get());

        epoc::skn_file skin_parser(reinterpret_cast<common::ro_stream *>(&skin_file_stream));
        chunk_maintainer_->import(skin_parser, resource_path.value_or(u""));
    }

    void akn_skin_server::do_initialisation() {
        kernel_system *kern = sys->get_kernel_system();
        server_ptr svr = kern->get_by_name<service::server>("!CentralRepository");

        settings_ = std::make_unique<epoc::akn_ss_settings>(sys->get_io_system(), !svr ? nullptr : reinterpret_cast<central_repo_server *>(&(*svr)));
        icon_config_map_ = std::make_unique<epoc::akn_skin_icon_config_map>(!svr ? nullptr : reinterpret_cast<central_repo_server *>(&(*svr)),
            sys->get_device_manager(), sys->get_io_system(), sys->get_system_language());

        fbss = reinterpret_cast<fbs_server *>(&(*kern->get_by_name<service::server>(
            epoc::get_fbs_server_name_by_epocver(kern->get_epoc_version()))));

        skin_chunk_ = kern->create_and_add<kernel::chunk>(kernel::owner_type::kernel,
                              sys->get_memory_system(), nullptr, "AknsSrvSharedMemoryChunk",
                              0, 384 * 1024, 384 * 1024, prot_read_write, kernel::chunk_type::normal,
                              kernel::chunk_access::global, kernel::chunk_attrib::none)
                          .second;

        kern->create_and_add<kernel::semaphore>(kernel::owner_type::kernel,
            nullptr, "AknsSrvWaitSemaphore", 0, kernel::access_type::global_access);

        kern->create_and_add<kernel::semaphore>(kernel::owner_type::kernel,
            nullptr, "AknsSrvRenderSemaphore", 0, kernel::access_type::global_access);

        std::uint32_t flags = 0;
        if (sys->get_symbian_version_use() > epocver::epoc94) {
            flags |= epoc::akn_skin_chunk_maintainer_lookup_use_linked_list;
        } else if (sys->get_symbian_version_use() == epocver::epoc94) {
            const std::u16string dll_path = u"z:\\sys\\bin\\aknskinsrv.dll";
            symfile dll_file = sys->get_io_system()->open_file(dll_path, READ_MODE | BIN_MODE | PREFER_PHYSICAL);
            if (dll_file) {
                ro_file_stream dll_file_stream(dll_file.get());
                if (loader::is_e32img(&dll_file_stream)) {
                    flags |= epoc::akn_skin_chunk_maintainer_lookup_use_linked_list;
                }
            }
        }

        chunk_maintainer_ = std::make_unique<epoc::akn_skin_chunk_maintainer>(skin_chunk_, 4 * 1024, flags);
        merge_active_skin(sys->get_io_system());
    }
}