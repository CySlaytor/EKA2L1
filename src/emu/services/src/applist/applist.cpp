#include <services/applist/applist.h>
#include <services/applist/op.h>
#include <services/context.h>
#include <services/fbs/fbs.h>
#include <services/fs/fs.h>

#include <common/benchmark.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/pystr.h>
#include <common/types.h>

#include <common/common.h>
#include <kernel/kernel.h>
#include <loader/rsc.h>
#include <system/epoc.h>
#include <utils/apacmd.h>
#include <utils/bafl.h>
#include <utils/des.h>
#include <vfs/vfs.h>

#include <config/config.h>
#include <functional>
#include <utils/err.h>

namespace eka2l1 {
    static const std::array<std::u16string, 6> RECOG_MIME_TYPES = {
        u"image/png", u"image/jpeg", u"image/bmp", u"audio/mpeg", u"video/mp4", u"application/octet-stream"
    };

    static void serialize_mime_arrays(common::chunkyseri &seri) {
        utils::cardinality card(static_cast<std::uint32_t>(RECOG_MIME_TYPES.size()));
        card.serialize(seri);
        std::uint32_t uid = 0;
        for (auto mime : RECOG_MIME_TYPES) {
            epoc::absorb_des_string(mime, seri, true);
            seri.absorb(uid);
        }
    }

    static void populate_icon_sizes(common::chunkyseri &seri, apa_app_registry *reg) {
        std::uint32_t size = reg->app_icons.size();
        seri.absorb(size);
        for (const auto &icon : reg->app_icons) {
            seri.absorb(icon.bmp_->bitmap_->header_.size_pixels.x);
            seri.absorb(icon.bmp_->bitmap_->header_.size_pixels.y);
        }
    }

    const std::string get_app_list_server_name_by_epocver(const epocver ver) {
        if (ver < epocver::epoc81a) {
            return "AppListServer";
        }
        return "!AppListServer";
    }

    applist_server::applist_server(system *sys)
        : service::typical_server(sys, get_app_list_server_name_by_epocver(sys->get_symbian_version_use()))
        , drive_change_handle_(0)
        , fbsserv(nullptr)
        , fsserv(nullptr)
        , loading_thread_pool_(std::thread::hardware_concurrency() <= 0 ? 4 : std::thread::hardware_concurrency()) {
    }

    applist_server::~applist_server() {
        io_system *io = sys->get_io_system();
        io->remove_drive_change_notify(drive_change_handle_);
    }

    bool applist_server::load_registry(eka2l1::io_system *io, const std::u16string &path, drive_number land_drive,
        const language ideal_lang) {
        const std::u16string nearest_path = utils::get_nearest_lang_file(io, path, ideal_lang, land_drive);
        symfile f = io->open_file(nearest_path, READ_MODE | BIN_MODE);
        if (!f)
            return false;

        std::uint64_t last_modified = 0;
        {
            const std::lock_guard<std::mutex> guard(list_access_mut_);
            auto find_result = std::find_if(regs.begin(), regs.end(), [=](const apa_app_registry &reg) {
                return (common::compare_ignore_case(reg.rsc_path, nearest_path) == 0);
            });
            last_modified = f->last_modify_since_0ad();
            if (find_result != regs.end()) {
                if (find_result->last_rsc_modified != last_modified) {
                    regs.erase(find_result);
                } else {
                    return false;
                }
            }
        }

        apa_app_registry reg;
        reg.land_drive = land_drive;
        reg.rsc_path = nearest_path;
        reg.last_rsc_modified = last_modified;

        auto read_rsc_from_file = [](symfile &f, const int id, const bool confirm_sig, std::uint32_t *uid3) -> std::vector<std::uint8_t> {
            eka2l1::ro_file_stream std_rsc_raw(f.get());
            if (!std_rsc_raw.valid())
                return {};
            loader::rsc_file std_rsc(reinterpret_cast<common::ro_stream *>(&std_rsc_raw));
            if (confirm_sig)
                std_rsc.confirm_signature();
            if (uid3)
                *uid3 = std_rsc.get_uid(3);
            return std_rsc.read(id);
        };

        auto dat = read_rsc_from_file(f, 1, false, &reg.mandatory_info.uid);
        if (dat.empty())
            return false;

        common::ro_buf_stream app_info_resource_stream(&dat[0], dat.size());
        bool result = read_registeration_info(reinterpret_cast<common::ro_stream *>(&app_info_resource_stream),
            reg, land_drive, kern->get_epoc_version() < epocver::epoc95);
        if (!result)
            return false;

        if (reg.localised_info_rsc_path.empty()) {
            reg.localised_info_rsc_path.append(1, drive_to_char16(land_drive));
            reg.localised_info_rsc_path += u":\\resource\\apps\\";
            std::u16string name_rsc = eka2l1::replace_extension(eka2l1::filename(reg.rsc_path), u"");
            if (common::lowercase_ucs2_string(name_rsc.substr(name_rsc.length() - 4, 4)) == u"_reg") {
                name_rsc.erase(name_rsc.length() - 4, 4);
            }
            reg.localised_info_rsc_path += name_rsc + u".rsc";
        }

        reg.localised_info_rsc_path = eka2l1::absolute_path(reg.localised_info_rsc_path,
            eka2l1::root_dir(path), true);
        const auto localised_path = utils::get_nearest_lang_file(io, reg.localised_info_rsc_path,
            ideal_lang, land_drive);

        if (localised_path.empty())
            return true;

        f = io->open_file(localised_path, READ_MODE | BIN_MODE);
        dat = read_rsc_from_file(f, reg.localised_info_rsc_id, true, nullptr);
        common::ro_buf_stream localised_app_info_resource_stream(&dat[0], dat.size());
        if (localised_app_info_resource_stream.valid()) {
            read_localised_registration_info(reinterpret_cast<common::ro_stream *>(&localised_app_info_resource_stream),
                reg, land_drive);
        }

        if (!eka2l1::is_absolute(reg.icon_file_path, std::u16string(u"c:\\"), true)) {
            std::u16string try_1 = eka2l1::absolute_path(reg.icon_file_path,
                std::u16string(1, drive_to_char16(land_drive)) + u":\\", true);
            if (io->exist(try_1)) {
                reg.icon_file_path = try_1;
            } else {
                try_1[0] = localised_path[0];
                if (io->exist(try_1)) {
                    reg.icon_file_path = try_1;
                } else {
                    reg.icon_file_path = u"";
                }
            }
        }

        const std::lock_guard<std::mutex> guard(list_access_mut_);
        regs.push_back(std::move(reg));
        return true;
    }

    void applist_server::sort_registry_list() {
        std::sort(regs.begin(), regs.end(), [](const apa_app_registry &lhs, const apa_app_registry &rhs) {
            return lhs.mandatory_info.uid < rhs.mandatory_info.uid;
        });
    }

    void applist_server::rescan_registries_on_drive_newarch(eka2l1::io_system *io, const drive_number drv,
        std::vector<std::u16string> &register_file_paths) {
        const std::u16string import_rsc_path = std::u16string(1, drive_to_char16(drv)) + u":\\Private\\10003a3f\\import\\apps\\*.r??";
        const std::u16string rom_rscs_path = std::u16string(1, drive_to_char16(drv)) + u":\\Private\\10003a3f\\apps\\*.r??";
        rescan_registries_on_drive_newarch_with_path(io, drv, rom_rscs_path, register_file_paths);
        rescan_registries_on_drive_newarch_with_path(io, drv, import_rsc_path, register_file_paths);
    }

    void applist_server::rescan_registries_on_drive_newarch_with_path(eka2l1::io_system *io, const drive_number drv, const std::u16string &path,
        std::vector<std::u16string> &results) {
        auto reg_dir = io->open_dir(path, {}, io_attrib_include_file);
        if (reg_dir) {
            while (auto ent = reg_dir->get_next_entry()) {
                if (ent->type == io_component_type::file) {
                    results.push_back(common::utf8_to_ucs2(ent->full_path));
                }
            }
        }
    }

    bool applist_server::rescan_registries(eka2l1::io_system *io) {
        std::atomic_bool global_modified = false;
        if (avail_drives_ == 0) {
            for (drive_number drv = drive_z; drv >= drive_a; drv--) {
                if (io->get_drive_entry(drv)) {
                    avail_drives_ |= 1 << (drv - drive_a);
                }
            }
        }

        std::size_t prev = regs.size();
        common::erase_elements(regs, [io](const apa_app_registry &reg) {
            return !io->exist(reg.rsc_path);
        });
        if (prev != regs.size())
            global_modified = true;

        std::vector<std::u16string> register_file_paths;
        for (std::uint8_t i = 0; i < drive_count; i++) {
            if (avail_drives_ & (1 << i)) {
                drive_number drv = static_cast<drive_number>(static_cast<int>(drive_a) + i);
                rescan_registries_on_drive_newarch(io, drv, register_file_paths);
            }
        }

        auto current_lang = kern->get_current_language();
        auto load_registry_task = loading_thread_pool_.submit_loop<std::size_t>(0, register_file_paths.size(),
            [this, &register_file_paths, &global_modified, io, current_lang](std::size_t idx) {
                bool modified = false;
                auto path = register_file_paths[idx];
                drive_number drv = char16_to_drive(path[0]);
                modified = load_registry(io, register_file_paths[idx], drv, current_lang);
                if (modified)
                    global_modified = true;
            });

        load_registry_task.wait();

        if (global_modified) {
            sort_registry_list();
        }
        return global_modified.load();
    }

    int applist_server::legacy_level() {
        if (kern->get_epoc_version() < epocver::epoc7)
            return APA_LEGACY_LEVEL_OLD;
        else if (kern->get_epoc_version() < epocver::epoc81a)
            return APA_LEGACY_LEVEL_S60V2;
        else if (kern->get_epoc_version() < epocver::eka2)
            return APA_LEGACY_LEVEL_TRANSITION;
        return APA_LEGACY_LEVEL_MORDEN;
    }

    void applist_server::connect(service::ipc_context &ctx) {
        create_session<applist_session>(&ctx);
        server::connect(ctx);
    }

    void applist_server::init() {
        fbsserv = reinterpret_cast<fbs_server *>(kern->get_by_name<service::server>(
            epoc::get_fbs_server_name_by_epocver(kern->get_epoc_version())));
        fsserv = kern->get_by_name<eka2l1::fs_server>(epoc::fs::get_server_name_through_epocver(
            kern->get_epoc_version()));
        rescan_registries(sys->get_io_system());
        flags |= AL_INITED;
    }

    apa_app_registry *applist_server::get_registration(const std::uint32_t uid) {
        std::unique_lock<std::mutex> guard(list_access_mut_);
        if (!(flags & AL_INITED)) {
            guard.unlock();
            init();
            guard.lock();
        }
        auto result = std::lower_bound(regs.begin(), regs.end(), uid, [](const apa_app_registry &lhs, const std::uint32_t rhs) {
            return lhs.mandatory_info.uid < rhs;
        });
        if (result != regs.end() && result->mandatory_info.uid == uid) {
            return &(*result);
        }
        return nullptr;
    }

    void applist_server::get_app_info(service::ipc_context &ctx) {
        const epoc::uid app_uid = *ctx.get_argument_value<epoc::uid>(0);
        apa_app_registry *reg = get_registration(app_uid);
        if (!reg) {
            ctx.complete(epoc::error_not_found);
            return;
        }
        apa_app_info info_copy = reg->mandatory_info;
        auto map_to_host_ite = uids_app_to_executable.find(app_uid);
        if (map_to_host_ite != uids_app_to_executable.end()) {
            info_copy.app_path = std::u16string(MAPPED_EXECUTABLE_HEAD_STRING) + u"_" + map_to_host_ite->second + UNIQUE_MAPPED_EXTENSION_STRING;
        }
        ctx.write_data_to_descriptor_argument<apa_app_info>(1, info_copy);
        ctx.complete(epoc::error_none);
    }

    void applist_server::get_app_icon_file_name(service::ipc_context &ctx) {
        const epoc::uid app_uid = *ctx.get_argument_value<epoc::uid>(0);
        apa_app_registry *reg = get_registration(app_uid);
        if (!reg) {
            ctx.complete(epoc::error_not_found);
            return;
        }
        if (reg->icon_file_path.empty()) {
            ctx.complete(epoc::error_not_supported);
            return;
        }
        epoc::filename fname_des(reg->icon_file_path);
        ctx.write_data_to_descriptor_argument<epoc::filename>(1, fname_des);
        ctx.complete(epoc::error_none);
    }

    void applist_server::get_preferred_buf_size(service::ipc_context &ctx) {
        std::uint32_t buf_size = 0;
        ctx.complete(buf_size);
    }

    void applist_server::get_app_for_document_impl(service::ipc_context &ctx, const std::u16string &path) {
        applist_app_for_document app;
        app.uid = 0;
        app.data_type.uid = 0;

        config::state *conf = kern->get_config();
        if (conf && conf->mime_detection) {
            const std::u16string ext = eka2l1::path_extension(path);
            if (!ext.empty()) {
                if (common::compare_ignore_case(ext, u".swf") == 0) {
                    app.data_type.data_type.assign(nullptr, "application/x-shockwave-flash");
                } else {
                    app.data_type.data_type.assign(nullptr, common::uppercase_string(common::ucs2_to_utf8(ext.substr(1))));
                }
            } else {
                app.data_type.data_type.assign(nullptr, "UNK");
            }
        }
        ctx.write_data_to_descriptor_argument<applist_app_for_document>(0, app);
        ctx.complete(epoc::error_none);
    }

    void applist_server::get_app_for_document(service::ipc_context &ctx) {
        std::optional<std::u16string> path = ctx.get_argument_value<std::u16string>(2);
        if (!path.has_value()) {
            ctx.complete(epoc::error_argument);
            return;
        }
        get_app_for_document_impl(ctx, path.value());
    }

    applist_session::applist_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_ver)
        : typical_session(svr, client_ss_uid, client_ver)
        , filter_method_(APP_FILTER_NONE) {
    }

    void applist_session::fetch(service::ipc_context *ctx) {
        const int llevel = server<applist_server>()->legacy_level();
        switch (ctx->msg->function) {
        case applist_request_app_info:
            server<applist_server>()->get_app_info(*ctx);
            break;
        case applist_request_app_icon_filename:
            server<applist_server>()->get_app_icon_file_name(*ctx);
            break;
        case applist_request_preferred_buf_size:
            server<applist_server>()->get_preferred_buf_size(*ctx);
            break;
        case applist_request_app_for_document:
            server<applist_server>()->get_app_for_document(*ctx);
            break;
        default:
            ctx->complete(epoc::error_none);
            break;
        }
    }

    void applist_server::add_app_uid_to_host_launch_name(const epoc::uid app_uid, const std::u16string &host_launch_name) {
        uids_app_to_executable.emplace(app_uid, host_launch_name);
    }
}