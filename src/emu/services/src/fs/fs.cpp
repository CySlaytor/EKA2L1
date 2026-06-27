#include <clocale>
#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/pystr.h>
#include <common/wildcard.h>
#include <cwctype>
#include <kernel/kernel.h>
#include <memory>
#include <re2/re2.h>
#include <services/fs/fs.h>
#include <services/fs/op.h>
#include <services/fs/sec.h>
#include <services/fs/std.h>
#include <system/epoc.h>
#include <utils/des.h>
#include <utils/err.h>
#include <vfs/vfs.h>

namespace eka2l1 {
    bool check_path_capabilities_pass(const std::u16string &path, kernel::process *pr, epoc::security_policy &private_policy, epoc::security_policy &sys_policy, epoc::security_policy &resource_policy) {
        if (!pr->get_kernel_object_owner()->support_capabilities()) {
            return true;
        }

        const std::u16string path_lowered = common::lowercase_ucs2_string(path);
        eka2l1::path_iterator_16 iterator(path_lowered);

        std::int8_t policy_checK_type = -1;

        iterator++;

        if (iterator) {
            if (*iterator == u"private") {
                policy_checK_type = 0;
            } else if (*iterator == u"sys") {
                policy_checK_type = 1;
            } else if (*iterator == u"resource") {
                policy_checK_type = 2;
            }

            if (policy_checK_type != -1) {
                if (iterator)
                    iterator++;
            }
        }

        if ((!iterator) || (policy_checK_type == -1)) {
            return true;
        }

        switch (policy_checK_type) {
        case 0: {
            common::pystr16 hexstr(*iterator);
            const std::uint32_t sid_in_path = hexstr.as_int<std::uint32_t>(0, 16);
            if (sid_in_path != pr->get_sec_info().secure_id) {
                return pr->satisfy(private_policy);
            }
            return true;
        }
        case 1:
            return pr->satisfy(sys_policy);
        case 2:
            return pr->satisfy(resource_policy);
        default:
            break;
        }
        return true;
    }

    static std::u16string get_private_path_trim_uid(kernel::process *pr) {
        uint32_t uid = std::get<2>(pr->get_uid_type());
        std::string hex_id = common::uppercase_string(common::to_string(uid, std::hex));
        return u"\\Private\\" + common::utf8_to_ucs2(hex_id) + u"\\";
    }

    static std::u16string get_private_path(kernel::process *pr, const drive_number drive) {
        const char16_t drive_dos_char = drive_to_char16(drive);
        const std::u16string drive_u16 = std::u16string(&drive_dos_char, 1) + u":";
        return drive_u16 + get_private_path_trim_uid(pr);
    }

    std::u16string get_full_symbian_path(const std::u16string &session_path, const std::u16string &target_path) {
        if (target_path.empty()) {
            return session_path;
        }
        if (is_separator(target_path[0])) {
            return common::trim_spaces(eka2l1::absolute_path(target_path, eka2l1::root_name(session_path, true), true));
        }
        if (!eka2l1::has_root_dir(target_path)) {
            return common::trim_spaces(eka2l1::absolute_path(target_path, session_path, true));
        }
        return common::trim_spaces(target_path);
    }

    size_t fs_path_case_insensitive_hasher::operator()(const utf16_str &key) const {
        utf16_str copy = common::lowercase_ucs2_string(key);
        return std::hash<utf16_str>()(copy);
    }

    bool fs_path_case_insensitive_comparer::operator()(const utf16_str &x, const utf16_str &y) const {
        return (common::compare_ignore_case(x, y) == -1);
    }

    fs_server_client::fs_server_client(service::typical_server *srv, kernel::uid suid, epoc::version client_version, kernel::thread *own_thr)
        : typical_session(srv, suid, client_version) {
        kernel::process *pr = own_thr ? own_thr->owning_process() : nullptr;
        if (server<fs_server>()->kern->is_eka1() || !pr) {
            ss_path = server<fs_server>()->default_sys_path;
        } else {
            ss_path = get_private_path(pr, static_cast<drive_number>(server<fs_server>()->system_drive_prop->get_int()));
        }
    }

    fs_server::fs_server(system *sys)
        : service::typical_server(sys, epoc::fs::get_server_name_through_epocver(sys->get_symbian_version_use()))
        , flags(0) {
        default_sys_path = u"C:\\";
        system_drive_prop = &(*sys->get_kernel_system()->create<service::property>());
        system_drive_prop->define(service::property_type::int_data, 0);
        system_drive_prop->set_int(drive_c);
        system_drive_prop->first = static_cast<int>(FS_UID);
        system_drive_prop->second = static_cast<int>(SYSTEM_DRIVE_KEY);
    }

    fs_server::~fs_server() {
        io_system *io = sys->get_io_system();
        for (const std::u16string &path : temporary_file_cleanset_) {
            io->delete_entry(path);
        }
    }

    void fs_server_client::fetch(service::ipc_context *ctx) {
        const epocver version = server<fs_server>()->sys->get_symbian_version_use();
        if (version < epocver::eka2) {
            if ((ctx->msg->function >= epoc::fs_msg_transition_begin) && (version >= epocver::epoc81a)) {
                const epoc::fs_message quick_lookup[] = {
                    epoc::fs_msg_swap_filesystem,
                    epoc::fs_msg_add_ext,
                    epoc::fs_msg_mount_ext,
                    epoc::fs_msg_dismount_ext,
                    epoc::fs_msg_remove_ext,
                    epoc::fs_msg_ext_name,
                    epoc::fs_msg_startup_init_complete,
                    epoc::fs_msg_raw_disk_write,
                    epoc::fs_msg_max_client_operations,
                    epoc::fs_msg_session_to_private,
                    epoc::fs_msg_private_path,
                    epoc::fs_msg_create_private_path,
                    epoc::fs_msg_reserve_drive_space,
                    epoc::fs_msg_get_reserve_access,
                    epoc::fs_msg_release_reserve_access,
                    epoc::fs_msg_erase_password,
                    epoc::fs_msg_max_client_operations,
                    epoc::fs_msg_max_client_operations,
                    epoc::fs_msg_max_client_operations,
                    epoc::fs_msg_max_client_operations,
                    epoc::fs_msg_max_client_operations,
                    epoc::fs_msg_max_client_operations
                };
                ctx->msg->function = quick_lookup[ctx->msg->function - epoc::fs_msg_transition_begin];
            } else {
                if (ctx->msg->function > epoc::fs_msg_base_close) {
                    ctx->msg->function += epoc::fs_msg_raw_subclose - epoc::fs_msg_base_close;
                }
                if (ctx->msg->function > epoc::fs_msg_debug_function) {
                    ctx->msg->function -= 1;
                }
            }
        } else {
            if (version == epocver::epoc93fp2) {
                if (ctx->msg->function >= epoc::fs_msg_file_write_dirty)
                    ctx->msg->function++;
            } else if (version == epocver::epoc93fp1) {
                if (ctx->msg->function >= epoc::fs_msg_set_system_drive) {
                    ctx->msg->function += epoc::fs_msg_file_write_dirty - epoc::fs_msg_set_system_drive + 1;
                }
            }
        }

        switch (ctx->msg->function & 0xFF) {
#define HANDLE_CLIENT_IPC(name, op, debug_func_str) \
    case (op): {                                    \
        name(ctx);                                  \
        break;                                      \
    }

            HANDLE_CLIENT_IPC(entry, epoc::fs_msg_entry, "Fs::Entry");
            HANDLE_CLIENT_IPC(file_open, epoc::fs_msg_file_open, "Fs::FileOpen");
            HANDLE_CLIENT_IPC(file_size, epoc::fs_msg_file_size, "Fs::FileSize");
            HANDLE_CLIENT_IPC(file_set_size, epoc::fs_msg_file_set_size, "Fs::FileSetSize");
            HANDLE_CLIENT_IPC(file_seek, epoc::fs_msg_file_seek, "Fs::FileSeek");
            HANDLE_CLIENT_IPC(file_read, epoc::fs_msg_file_read, "Fs::FileRead");
            HANDLE_CLIENT_IPC(file_write, epoc::fs_msg_file_write, "Fs::FileWrite");
            HANDLE_CLIENT_IPC(file_flush, epoc::fs_msg_file_flush, "Fs::FileFlush");
            HANDLE_CLIENT_IPC(file_create, epoc::fs_msg_file_create, "Fs::FileCreate");
            HANDLE_CLIENT_IPC(file_replace, epoc::fs_msg_file_replace, "Fs::FileReplace");
            HANDLE_CLIENT_IPC(file_close, epoc::fs_msg_file_subclose, "Fs::FileSubClose");
            HANDLE_CLIENT_IPC(file_drive, epoc::fs_msg_file_drive, "Fs::FileDrive");
            HANDLE_CLIENT_IPC(file_name, epoc::fs_msg_filename, "Fs::FileName");
            HANDLE_CLIENT_IPC(file_att, epoc::fs_msg_file_att, "Fs::FileAtt");
            HANDLE_CLIENT_IPC(file_modified, epoc::fs_msg_file_modified, "Fs::FileModified");
            HANDLE_CLIENT_IPC(is_file_in_rom, epoc::fs_msg_is_file_in_rom, "Fs::IsFileInRom");
            HANDLE_CLIENT_IPC(open_dir, epoc::fs_msg_dir_open, "Fs::OpenDir");
            HANDLE_CLIENT_IPC(close_dir, epoc::fs_msg_dir_subclose, "Fs::CloseDir");
            HANDLE_CLIENT_IPC(read_dir_packed, epoc::fs_msg_dir_read_packed, "Fs::ReadDirPacked");
            HANDLE_CLIENT_IPC(session_path, epoc::fs_msg_session_path, "Fs::SessionPath");
            HANDLE_CLIENT_IPC(set_session_path, epoc::fs_msg_set_session_path, "Fs::SetSessionPath");
            HANDLE_CLIENT_IPC(set_session_to_private, epoc::fs_msg_session_to_private, "Fs::SetSessionToPrivate");
            HANDLE_CLIENT_IPC(create_private_path, epoc::fs_msg_create_private_path, "Fs::CreatePrivatePath");
            HANDLE_CLIENT_IPC(notify_change_ex, epoc::fs_msg_notify_change_ex, "Fs::NotifyChangeEx");
            HANDLE_CLIENT_IPC(notify_change_cancel, epoc::fs_msg_notify_change_cancel, "Fs::NotifyChangeCancel");
            HANDLE_CLIENT_IPC(notify_change_cancel_ex, epoc::fs_msg_notify_change_cancel_ex, "Fs::NotifyChangeCancelEx");
            HANDLE_CLIENT_IPC(delete_entry, epoc::fs_msg_delete, "Fs::Delete");
            HANDLE_CLIENT_IPC(rename, epoc::fs_msg_rename, "Fs::Rename(Move)");
            HANDLE_CLIENT_IPC(drive_list, epoc::fs_msg_drive_list, "Fs::DriveList");
            HANDLE_CLIENT_IPC(drive, epoc::fs_msg_drive, "Fs::Drive");
            HANDLE_CLIENT_IPC(server<fs_server>()->synchronize_driver, epoc::fs_msg_sync_drive_thread, "Fs::SyncDriveThread");
            HANDLE_CLIENT_IPC(server<fs_server>()->private_path, epoc::fs_msg_private_path, "Fs::PrivatePath");
            HANDLE_CLIENT_IPC(volume, epoc::fs_msg_volume, "Fs::Volume");
            HANDLE_CLIENT_IPC(notify_dismount, epoc::fs_msg_notify_dismount, "Fs::NotifyDismount");
            HANDLE_CLIENT_IPC(notify_dismount_cancel, epoc::fs_msg_notify_dismount_cancel, "Fs::NotifyDismountCancel");
            HANDLE_CLIENT_IPC(set_entry, epoc::fs_msg_set_entry, "Fs::SetEntry");

        case epoc::fs_msg_base_close:
            if (ctx->sys->get_symbian_version_use() < epocver::eka2) {
                ctx->complete(epoc::error_none);
            }
            break;

        case epoc::fs_msg_notification_open:
        case epoc::fs_msg_notification_buffer:
        case epoc::fs_msg_notification_remove:
        case epoc::fs_msg_notification_cancel:
        case epoc::fs_msg_notification_request:
        case epoc::fs_msg_notification_subclose:
            ctx->complete(epoc::error_none);
            break;

        default: {
            LOG_ERROR(SERVICE_EFSRV, "Unknown FSServer client opcode {}!", ctx->msg->function);
            ctx->complete(epoc::error_none);
            break;
        }
#undef HANDLE_CLIENT_IPC
        }
    }

    void fs_server_client::rename(service::ipc_context *ctx) {
        auto given_path_target = ctx->get_argument_value<std::u16string>(0);
        auto given_path_dest = ctx->get_argument_value<std::u16string>(1);

        if (!given_path_target || !given_path_dest) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::u16string target = get_full_symbian_path(ss_path, given_path_target.value());
        std::u16string dest = get_full_symbian_path(ss_path, given_path_dest.value());
        io_system *io = ctx->sys->get_io_system();

        if (!io->exist(target)) {
            if (eka2l1::is_separator(target.back())) {
                ctx->complete(epoc::error_path_not_found);
            } else {
                ctx->complete(epoc::error_not_found);
            }
            return;
        }

        if (io->exist(dest)) {
            ctx->complete(epoc::error_already_exists);
            return;
        }

        bool res = io->rename(target, dest);
        if (!res) {
            ctx->complete(epoc::error_general);
            return;
        }
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::delete_entry(service::ipc_context *ctx) {
        auto given_path = ctx->get_argument_value<std::u16string>(0);
        if (!given_path) {
            ctx->complete(epoc::error_argument);
            return;
        }

        auto path = get_full_symbian_path(ss_path, given_path.value());
        if (!check_path_capabilities_pass(path, ctx->msg->own_thr->owning_process(), epoc::fs::private_comp_access_policy, epoc::fs::sys_resource_modify_access_policy, epoc::fs::sys_resource_modify_access_policy)) {
            ctx->complete(epoc::error_permission_denied);
            return;
        }

        io_system *io = ctx->sys->get_io_system();
        bool success = io->delete_entry(path);

        if (!success) {
            ctx->complete(epoc::error_not_found);
            return;
        }
        ctx->complete(epoc::error_none);
    }

    void fs_server::synchronize_driver(service::ipc_context *ctx) {
        ctx->complete(epoc::error_none);
    }

    void fs_server::init() {
        if (flags & FLAG_INITED) {
            return;
        }

        if (kern->is_eka1()) {
            std::u16string system_apps_dir = u"?:\\System\\Apps\\";
            std::u16string shared_data_dir = u"?:\\System\\SharedData\\";
            io_system *io = sys->get_io_system();

            for (drive_number drv = drive_y; drv >= drive_a; drv--) {
                if (io->get_drive_entry(drv)) {
                    system_apps_dir[0] = drive_to_char16(drv);
                    shared_data_dir[0] = drive_to_char16(drv);
                    io->create_directories(system_apps_dir);
                    io->create_directories(shared_data_dir);
                }
            }
        }

        std::u16string temp_data_dir = u"?:\\System\\Temp\\";
        io_system *io = sys->get_io_system();

        for (drive_number drv = drive_y; drv >= drive_a; drv--) {
            if (io->get_drive_entry(drv)) {
                temp_data_dir[0] = drive_to_char16(drv);
                if (auto dir = io->open_dir(temp_data_dir)) {
                    while (auto dir_entry = dir->get_next_entry()) {
                        if (dir_entry->type == io_component_type::file) {
                            io->delete_entry(common::utf8_to_ucs2(dir_entry->full_path));
                        }
                    }
                } else {
                    io->create_directories(temp_data_dir);
                }
            }
        }
        flags |= FLAG_INITED;
    }

    void fs_server::connect(service::ipc_context &ctx) {
        if (!(flags & FLAG_INITED)) {
            init();
        }
        fs_server_client *cli = create_session<fs_server_client>(&ctx, ctx.msg->own_thr);
        typical_server::connect(ctx);
    }

    void fs_server::disconnect(service::ipc_context &ctx) {
        typical_server::disconnect(ctx);
    }

    fs_server_client *fs_server::get_correspond_client(service::session *ss) {
        if (!(flags & FLAG_INITED)) {
            init();
        }
        auto result = sessions.find(ss->unique_id());
        if (result != sessions.end()) {
            return reinterpret_cast<fs_server_client *>(result->second.get());
        }
        return create_session_impl<fs_server_client>(ss->unique_id(), epoc::version{ 0 }, nullptr);
    }

    void fs_server_client::session_path(service::ipc_context *ctx) {
        ctx->write_arg(0, ss_path);
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::set_session_path(service::ipc_context *ctx) {
        auto new_path = ctx->get_argument_value<std::u16string>(0);
        if (!new_path) {
            ctx->complete(epoc::error_argument);
            return;
        }
        ss_path = std::move(new_path.value());
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::set_session_to_private(service::ipc_context *ctx) {
        auto drive_ordinal = ctx->get_argument_value<std::int32_t>(0);
        if (!drive_ordinal) {
            ctx->complete(epoc::error_argument);
            return;
        }
        ss_path = get_private_path(ctx->msg->own_thr->owning_process(), static_cast<drive_number>(drive_ordinal.value()));
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::create_private_path(service::ipc_context *ctx) {
        const std::u16string private_path = get_private_path(ctx->msg->own_thr->owning_process(),
            static_cast<drive_number>(ctx->get_argument_value<std::int32_t>(0).value()));
        eka2l1::io_system *io = ctx->sys->get_io_system();

        if (io->exist(private_path)) {
            ctx->complete(epoc::error_none);
            return;
        }
        if (!io->create_directory(private_path)) {
            ctx->complete(epoc::error_permission_denied);
            return;
        }
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::is_file_in_rom(service::ipc_context *ctx) {
        std::optional<utf16_str> path = ctx->get_argument_value<utf16_str>(0);
        if (!path) {
            ctx->complete(epoc::error_argument);
            return;
        }
        auto final_path = get_full_symbian_path(ss_path, path.value());
        symfile f = ctx->sys->get_io_system()->open_file(final_path, READ_MODE);

        if (!f) {
            ctx->complete(0);
            return;
        }

        address addr = f->rom_address();
        f->close();

        ctx->write_data_to_descriptor_argument<address>(1, addr);
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::notify_change_ex(service::ipc_context *ctx) {
        kernel_system *kern = ctx->sys->get_kernel_system();
        std::optional<utf16_str> wildcard_match = ctx->get_argument_value<utf16_str>(1);
        if (!wildcard_match) {
            ctx->complete(epoc::error_argument);
            return;
        }

        notify_entry entry;
        entry.match_pattern = common::wildcard_to_regex_string(common::ucs2_to_utf8(*wildcard_match));
        entry.type = static_cast<notify_type>(*ctx->get_argument_value<std::int32_t>(0));
        entry.info = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);

        if (kern->is_eka1()) {
            std::optional<address> notify_reqaddr = ctx->get_argument_value<address>(2);
            if (!notify_reqaddr.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }
            entry.info.sts = notify_reqaddr.value();
            ctx->complete(epoc::error_none);
        }

        notify_entries.push_back(entry);
        LOG_TRACE(SERVICE_EFSRV, "Notify requested with wildcard: {}", common::ucs2_to_utf8(*wildcard_match));
    }

    void fs_server_client::notify_change_cancel(service::ipc_context *ctx) {
        for (auto it = notify_entries.begin(); it != notify_entries.end(); ++it) {
            it->info.complete(epoc::error_cancel);
        }
        notify_entries.clear();
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::notify_change_cancel_ex(service::ipc_context *ctx) {
        address request_status_addr = ctx->get_argument_value<address>(0).value();
        for (auto it = notify_entries.begin(); it != notify_entries.end(); ++it) {
            notify_entry entry = *it;
            if (entry.info.sts.ptr_address() == request_status_addr) {
                entry.info.complete(epoc::error_cancel);
                notify_entries.erase(it);
                break;
            }
        }
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::notify_dismount(service::ipc_context *ctx) {
        LOG_TRACE(SERVICE_EFSRV, "Notify dismount stubbed");
        dismount_notify_ = epoc::notify_info(ctx->msg->request_sts, ctx->msg->own_thr);
    }

    void fs_server_client::notify_dismount_cancel(service::ipc_context *ctx) {
        dismount_notify_.complete(epoc::error_cancel);
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::entry(service::ipc_context *ctx) {
        std::optional<std::u16string> fname_op = ctx->get_argument_value<std::u16string>(0);
        if (!fname_op) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::u16string fname = std::move(*fname_op);
        fname = get_full_symbian_path(ss_path, fname);

        if (!check_path_capabilities_pass(fname, ctx->msg->own_thr->owning_process(), epoc::fs::private_comp_access_policy, epoc::fs::sys_read_only_access_policy, epoc::fs::resource_read_only_access_policy)) {
            ctx->complete(epoc::error_permission_denied);
            return;
        }

        LOG_INFO(SERVICE_EFSRV, "Get entry of: {}", common::ucs2_to_utf8(fname));
        io_system *io = ctx->sys->get_io_system();
        std::optional<entry_info> entry_hle = io->get_entry_info(fname);

        if (!entry_hle) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        epoc::fs::entry entry;
        epoc::fs::build_symbian_entry_from_emulator_entry(io, entry_hle.value(), entry);

        ctx->write_data_to_descriptor_argument<epoc::fs::entry>(1, entry, nullptr, true);
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::set_entry(service::ipc_context *ctx) {
        std::optional<std::u16string> fname_op = ctx->get_argument_value<std::u16string>(0);
        if (!fname_op) {
            ctx->complete(epoc::error_argument);
            return;
        }

        const std::u16string fname = get_full_symbian_path(ss_path, fname_op.value());
        LOG_INFO(SERVICE_EFSRV, "Set entry of: {}", common::ucs2_to_utf8(fname));
        io_system *io = ctx->sys->get_io_system();
        std::optional<entry_info> entry_hle = io->get_entry_info(fname);

        if (!entry_hle) {
            ctx->complete(epoc::error_not_found);
            return;
        }
        ctx->complete(epoc::error_none);
    }

    void fs_server::private_path(service::ipc_context *ctx) {
        std::u16string path = get_private_path_trim_uid(ctx->msg->own_thr->owning_process());
        ctx->write_arg(0, path);
        ctx->complete(epoc::error_none);
    }
}