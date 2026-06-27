#include <services/fs/fs.h>
#include <services/fs/std.h>

#include <kernel/kernel.h>
#include <system/epoc.h>
#include <vfs/vfs.h>

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/random.h>
#include <utils/err.h>

#include <services/fs/sec.h>

namespace eka2l1 {
    bool file_attrib::claim_exclusive(const kernel::uid pr_uid) {
        if (owner == pr_uid) {
            flags |= static_cast<std::uint32_t>(fs_file_attrib_flag::exclusive);
            return true;
        }
        return false;
    }

    void file_attrib::increment_use(const kernel::uid pr_uid) {
        if (use_count == 0) {
            owner = pr_uid;
        }

        if (owner == pr_uid) {
            use_count++;
        }
    }

    void file_attrib::decrement_use(const kernel::uid pr_uid) {
        if (use_count == 0) {
            return;
        }

        if (pr_uid == owner) {
            use_count--;
        }

        if (use_count == 0) {
            flags = 0;
            owner = 0;
            return;
        }
    }

    fs_node::~fs_node() {
        if (temporary) {
            io_system *io = serv->get_system()->get_io_system();

            if (vfs_node->type == io_component_type::file) {
                file *vfs_file = reinterpret_cast<file *>(vfs_node.get());
                serv->temporary_file_cleanset_.emplace(common::lowercase_ucs2_string(vfs_file->file_name()));
            }
        }
    }

    void fs_node::deref() {
        if (vfs_node->type == io_component_type::file) {
            file *vfs_file = reinterpret_cast<file *>(vfs_node.get());
            const std::u16string filename = vfs_file->file_name();

            auto ite = serv->attribs.find(filename);
            if (ite != serv->attribs.end()) {
                ite->second.decrement_use(process);
                if (ite->second.use_count == 0) {
                    serv->attribs.erase(ite);
                }
            }
        }

        epoc::ref_count_object::deref();
    }

    symfile fs_server::get_temp_file(const std::u16string &base_dir) {
        std::u16string full_path = base_dir;
        io_system *io = sys->get_io_system();

        do {
            std::u16string temp_name{ u"temp" };
            temp_name += common::utf8_to_ucs2(common::to_string(eka2l1::random_range(0, 0xFFFFFFFE), std::hex));
            full_path = eka2l1::add_path(base_dir, temp_name);
            if (!io->exist(full_path)) {
                break;
            }
        } while (true);

        return io->open_file(full_path, WRITE_MODE | BIN_MODE);
    }

    void fs_server_client::file_modified(service::ipc_context *ctx) {
        std::optional<std::int32_t> handle_res = ctx->get_argument_value<std::int32_t>(3);
        if (!handle_res) {
            ctx->complete(epoc::error_argument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);
        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        file *the_file = reinterpret_cast<file *>(node->vfs_node.get());
        epoc::time last_modified{ the_file->last_modify_since_0ad() };

        ctx->write_data_to_descriptor_argument<epoc::time>(0, last_modified);
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::file_size(service::ipc_context *ctx) {
        std::optional<std::int32_t> handle_res = ctx->get_argument_value<std::int32_t>(3);
        if (!handle_res) {
            ctx->complete(epoc::error_argument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);
        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        if (ctx->sys->get_kernel_system()->get_epoc_version() >= epocver::epoc95) {
            std::uint64_t fsize_u = reinterpret_cast<file *>(node->vfs_node.get())->size();
            ctx->write_data_to_descriptor_argument<uint64_t>(0, fsize_u);
        } else {
            std::uint32_t fsize_u = static_cast<std::uint32_t>(reinterpret_cast<file *>(node->vfs_node.get())->size());
            ctx->write_data_to_descriptor_argument<uint32_t>(0, fsize_u);
        }

        ctx->complete(epoc::error_none);
    }

    void fs_server_client::file_set_size(service::ipc_context *ctx) {
        std::optional<int> handle_res = ctx->get_argument_value<std::int32_t>(3);
        if (!handle_res) {
            ctx->complete(epoc::error_argument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);
        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        if (!(node->open_mode & WRITE_MODE) && !(node->open_mode & APPEND_MODE)) {
            ctx->complete(epoc::error_permission_denied);
            return;
        }

        std::uint64_t size = 0;
        if (ctx->msg->function & 0x10000) {
            std::optional<std::uint64_t> size_opt = ctx->get_argument_data_from_descriptor<std::uint64_t>(0);
            if (!size_opt.value()) {
                ctx->complete(epoc::error_argument);
            } else {
                size = size_opt.value();
            }
        } else {
            size = static_cast<std::uint64_t>(ctx->get_argument_value<std::int32_t>(0).value());
        }

        file *f = reinterpret_cast<file *>(node->vfs_node.get());
        std::size_t fsize = f->size();

        if (size == fsize) {
            ctx->complete(epoc::error_none);
            return;
        }

        if (size >= common::GB(1)) {
            LOG_ERROR(SERVICE_EFSRV, "File trying to resize to 1GB, operation not permitted");
            ctx->complete(epoc::error_too_big);
            return;
        }

        const std::uint64_t prev_pos = f->tell();
        bool res = f->resize(size);

        if (!res) {
            ctx->complete(epoc::error_general);
            return;
        }

        if (prev_pos > size) {
            f->seek(size, file_seek_mode::beg);
        }

        ctx->complete(epoc::error_none);
    }

    void fs_server_client::file_name(service::ipc_context *ctx) {
        std::optional<std::int32_t> handle_res = ctx->get_argument_value<std::int32_t>(3);
        if (!handle_res) {
            ctx->complete(epoc::error_argument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);
        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        file *f = reinterpret_cast<file *>(node->vfs_node.get());
        ctx->write_arg(0, eka2l1::filename(f->file_name(), true));
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::file_seek(service::ipc_context *ctx) {
        std::optional<std::int32_t> handle_res = ctx->get_argument_value<std::int32_t>(3);
        if (!handle_res) {
            ctx->complete(epoc::error_argument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);
        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        file *vfs_file = reinterpret_cast<file *>(node->vfs_node.get());

        std::optional<std::int32_t> seek_mode = ctx->get_argument_value<std::int32_t>(1);
        std::optional<std::int64_t> seek_off = std::nullopt;

        if (ctx->msg->function & epoc::fs::ipc_arg_slot0_des) {
            seek_off = ctx->get_argument_data_from_descriptor<std::int64_t>(0);
        } else {
            seek_off = *ctx->get_argument_value<std::int32_t>(0);
        }

        if (!seek_mode || !seek_off) {
            ctx->complete(epoc::error_argument);
            return;
        }

        file_seek_mode vfs_seek_mode;
        switch (*seek_mode) {
        case 0:
            vfs_seek_mode = file_seek_mode::address;
            break;
        default:
            vfs_seek_mode = static_cast<file_seek_mode>(*seek_mode - 1);
            break;
        }

        std::uint64_t seek_res = vfs_file->seek(*seek_off, vfs_seek_mode);

        if (seek_res == 0xFFFFFFFFFFFFFFFF) {
            ctx->complete(epoc::error_argument);
            return;
        }

        if ((int)ctx->sys->get_symbian_version_use() >= (int)epocver::epoc95) {
            ctx->write_data_to_descriptor_argument(2, seek_res);
        } else {
            int seek_result_i = static_cast<std::int32_t>(seek_res);
            ctx->write_data_to_descriptor_argument(2, seek_result_i);
        }

        ctx->complete(epoc::error_none);
    }

    void fs_server_client::file_flush(service::ipc_context *ctx) {
        std::optional<std::int32_t> handle_res = ctx->get_argument_value<std::int32_t>(3);
        if (!handle_res) {
            ctx->complete(epoc::error_argument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);
        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        file *vfs_file = reinterpret_cast<file *>(node->vfs_node.get());

        if (!vfs_file->flush()) {
            LOG_ERROR(SERVICE_EFSRV, "Fail flushing file with path {}", common::ucs2_to_utf8(vfs_file->file_name()));
            ctx->complete(epoc::error_general);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    void fs_server_client::file_write(service::ipc_context *ctx) {
        std::optional<std::int32_t> handle_res = ctx->get_argument_value<std::int32_t>(3);
        if (!handle_res) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::optional<std::string> write_data = ctx->get_argument_value<std::string>(0);
        if (!write_data) {
            ctx->complete(epoc::error_argument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);
        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        file *vfs_file = reinterpret_cast<file *>(node->vfs_node.get());

        if (!(node->open_mode & WRITE_MODE)) {
            ctx->complete(epoc::error_access_denied);
            return;
        }

        std::int32_t write_len = *ctx->get_argument_value<std::int32_t>(1);
        std::int32_t write_pos_provided = *ctx->get_argument_value<std::int32_t>(2);

        std::uint64_t write_pos = 0;
        std::uint64_t size_of_file = vfs_file->size();

        write_pos = vfs_file->tell();

        if ((write_pos_provided != static_cast<int>(0x80000000)) && (write_pos_provided != -1)) {
            write_pos = write_pos_provided;
        }

        if (write_pos > size_of_file) {
            vfs_file->seek(0, file_seek_mode::end);
            static char ZERO_BYTE = 0;
            if (vfs_file->write_file(&ZERO_BYTE, 1, static_cast<std::uint32_t>(write_pos - size_of_file)) != write_pos - size_of_file) {
                LOG_WARN(SERVICE_EFSRV, "Unable to supply stubbed bytes for beyond file size write operation!");
            }
        }

        vfs_file->seek(write_pos, file_seek_mode::beg);
        size_t wrote_size = vfs_file->write_file(write_data.value().data(), 1, write_len);

        ctx->complete(epoc::error_none);
    }

    void fs_server_client::file_read(service::ipc_context *ctx) {
        std::optional<std::int32_t> handle_res = ctx->get_argument_value<std::int32_t>(3);
        if (!handle_res) {
            ctx->complete(epoc::error_argument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);
        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        file *vfs_file = reinterpret_cast<file *>(node->vfs_node.get());

        int read_len = *ctx->get_argument_value<std::int32_t>(1);
        std::uint64_t read_pos = 0;
        std::uint64_t last_pos = vfs_file->tell();

        read_pos = last_pos;

        if (ctx->msg->function & epoc::fs::ipc_arg_slot2_des) {
            std::optional<std::uint64_t> temp = ctx->get_argument_data_from_descriptor<std::uint64_t>(2);
            if (!temp.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }
            if (temp.value() != static_cast<std::uint64_t>(-1)) {
                read_pos = temp.value();
            }
        } else {
            std::int32_t pos_gotten = *ctx->get_argument_value<std::int32_t>(2);
            if ((pos_gotten != static_cast<std::int32_t>(0x80000000)) && (pos_gotten != -1)) {
                read_pos = static_cast<std::uint64_t>(pos_gotten);
            }
        }

        vfs_file->seek(read_pos, file_seek_mode::beg);
        uint64_t size = vfs_file->size();

        if (size - read_pos < read_len) {
            read_len = static_cast<int>(size - read_pos);
        }

        std::vector<char> read_data;
        read_data.resize(read_len);

        size_t read_finish_len = vfs_file->read_file(read_data.data(), 1, read_len);
        ctx->write_data_to_descriptor_argument(0, reinterpret_cast<uint8_t *>(read_data.data()), static_cast<std::uint32_t>(read_finish_len));

        ctx->complete(epoc::error_none);
    }

    void fs_server_client::file_close(service::ipc_context *ctx) {
        std::optional<std::int32_t> handle_res = ctx->get_argument_value<std::int32_t>(3);
        if (!handle_res) {
            ctx->complete(epoc::error_argument);
            return;
        }

        fs_node *node = get_file_node(*handle_res);
        if (node == nullptr || node->vfs_node->type != io_component_type::file) {
            ctx->complete(epoc::error_bad_handle);
            return;
        }

        file *vfs_file = reinterpret_cast<file *>(node->vfs_node.get());

        vfs_file->flush();
        vfs_file->seek(0, file_seek_mode::beg);

        obj_table_.remove(*handle_res);
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::file_open(service::ipc_context *ctx) {
        new_file_subsession(ctx, exist_mode_neccessary, false);
    }

    void fs_server_client::file_replace(service::ipc_context *ctx) {
        new_file_subsession(ctx, exist_mode_dont_care, true);
    }

    void fs_server_client::file_create(service::ipc_context *ctx) {
        new_file_subsession(ctx, exist_mode_must_not, true);
    }

    void fs_server_client::file_att(service::ipc_context *ctx) {
        std::int32_t target_handle = *ctx->get_argument_value<std::int32_t>(3);
        if (!target_handle) {
            ctx->complete(epoc::error_argument);
            return;
        }

        fs_node *node = get_file_node(target_handle);
        if (!node) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        file *f = reinterpret_cast<file *>(node->vfs_node.get());
        io_system *io = ctx->sys->get_io_system();

        std::optional<entry_info> info = io->get_entry_info(f->file_name());
        assert(info);

        const std::uint32_t attrib = epoc::fs::build_attribute_from_entry_info(info.value());
        ctx->write_data_to_descriptor_argument(0, attrib);
        ctx->complete(epoc::error_none);
    }

    void fs_server_client::read_file_section(service::ipc_context *ctx) {
        ctx->complete(epoc::error_not_supported);
    }

    void fs_server_client::file_duplicate(service::ipc_context *ctx) {
        ctx->complete(epoc::error_not_supported);
    }

    void fs_server_client::file_adopt(service::ipc_context *ctx) {
        ctx->complete(epoc::error_not_supported);
    }

    void fs_server_client::new_file_subsession(service::ipc_context *ctx, const exist_check_mode existence,
        bool overwrite, bool temporary) {
        std::optional<std::u16string> name_res = ctx->get_argument_value<std::u16string>(0);
        std::optional<std::int32_t> open_mode_res = ctx->get_argument_value<std::int32_t>(1);

        if (!name_res || !open_mode_res) {
            ctx->complete(epoc::error_argument);
            return;
        }

        *name_res = get_full_symbian_path(ss_path, *name_res);
        std::string name_utf8 = common::ucs2_to_utf8(*name_res);
        io_system *io = ctx->sys->get_io_system();

        {
            auto file_dir = eka2l1::file_directory(*name_res);
            if (!io->exist(file_dir)) {
                LOG_TRACE(SERVICE_EFSRV, "Base directory of file {} not found", name_utf8);
                ctx->complete(epoc::error_path_not_found);
                return;
            }
        }

        const bool is_it_avail = io->exist(name_res.value());

        if (is_it_avail && (existence == exist_mode_must_not)) {
            LOG_ERROR(SERVICE_EFSRV, "Trying to open existing file: {}, while the requirement open mode forbidded this!", name_utf8);
            ctx->complete(epoc::error_already_exists);
            return;
        }

        if (!is_it_avail && (existence == exist_mode_neccessary)) {
            LOG_ERROR(SERVICE_EFSRV, "Trying to open a non-existent file: {} while the open mode requires its availbility!", name_utf8);
            ctx->complete(epoc::error_not_found);
            return;
        }

        LOG_INFO(SERVICE_EFSRV, "Opening file: {}, raw mode {}", name_utf8, open_mode_res.value());
        int handle = new_node(ctx->sys->get_io_system(), ctx->msg->own_thr, *name_res,
            *open_mode_res, overwrite, temporary);

        if (handle <= 0) {
            ctx->complete(handle);
            return;
        }

        LOG_TRACE(SERVICE_EFSRV, "Handle opened: {}", handle);

        ctx->write_data_to_descriptor_argument<int>(3, handle);
        ctx->complete(epoc::error_none);
    }

    int fs_server_client::new_node(io_system *io, kernel::thread *sender, std::u16string name, int org_mode, bool overwrite, bool temporary, bool ignore_caps) {
        if (name.empty() || (eka2l1::is_separator(name.back()))) {
            LOG_ERROR(SERVICE_EFSRV, "Path is invalid!");
            return epoc::error_bad_name;
        }

        int real_mode = org_mode & ~(epoc::fs::file_stream_text | epoc::fs::file_read_async_all | epoc::fs::file_big_size);
        epoc::fs::file_mode share_mode = static_cast<epoc::fs::file_mode>(real_mode & 0b11);

        int access_mode = -1;
        if (!(real_mode & epoc::fs::file_stream_text)) {
            access_mode = BIN_MODE;
        } else {
            access_mode = 0;
        }

        if (real_mode & epoc::fs::file_write) {
            if (!overwrite) {
                access_mode |= (READ_MODE | WRITE_MODE);
            } else {
                access_mode |= WRITE_MODE;
            }
        } else {
            access_mode |= (overwrite ? WRITE_MODE : READ_MODE);
        }

        if ((access_mode & WRITE_MODE) && (share_mode == epoc::fs::file_share_readers_only)) {
            return epoc::error_argument;
        }

        kernel::process *own_pr = sender->owning_process();
        kernel::uid own_pr_uid = own_pr->unique_id();

        if (!ignore_caps) {
            if (!check_path_capabilities_pass(name, own_pr, epoc::fs::private_comp_access_policy, (access_mode & WRITE_MODE) ? epoc::fs::sys_resource_modify_access_policy : epoc::fs::sys_read_only_access_policy,
                    (access_mode & WRITE_MODE) ? epoc::fs::sys_resource_modify_access_policy : epoc::fs::resource_read_only_access_policy)) {
                LOG_ERROR(SERVICE_EFSRV, "New file node fails capabilities requirements with path={}, process UID=0x{:X}", common::ucs2_to_utf8(name), own_pr_uid);
                return epoc::error_permission_denied;
            }
        }

        fs_node *new_node = server<fs_server>()->make_new<fs_node>();
        auto &node_attrib = server<fs_server>()->attribs[name];

        if (node_attrib.is_exlusive()) {
            if (node_attrib.owner != own_pr_uid) {
                return epoc::error_in_use;
            }
        }

        node_attrib.increment_use(own_pr_uid);

        if (share_mode == epoc::fs::file_share_exclusive) {
            if (!node_attrib.claim_exclusive(own_pr_uid)) {
                return epoc::error_in_use;
            }
        }

        if (node_attrib.is_readable_and_writeable() && !node_attrib.is_optional()) {
            if (share_mode == epoc::fs::file_share_readers_only) {
                return epoc::error_access_denied;
            }
        }

        if (node_attrib.is_readonly() && ((share_mode == epoc::fs::file_share_any) || (share_mode == epoc::fs::file_share_readers_or_writers))) {
            return epoc::error_access_denied;
        }

        new_node->vfs_node = io->open_file(name, access_mode);

        if (!new_node->vfs_node) {
            LOG_TRACE(SERVICE_EFSRV, "Can't open file {}", common::ucs2_to_utf8(name));
            return epoc::error_not_found;
        }

        new_node->mix_mode = real_mode;
        new_node->open_mode = access_mode;
        new_node->process = own_pr_uid;
        new_node->temporary = temporary;
        new_node->serv = server<fs_server>();

        return obj_table_.add(new_node);
    }
}