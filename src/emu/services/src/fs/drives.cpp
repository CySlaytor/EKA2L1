#include <services/fs/fs.h>
#include <services/fs/std.h>

#include <system/epoc.h>
#include <utils/err.h>
#include <vfs/vfs.h>

#include <cwctype>

namespace eka2l1 {
    void fill_drive_info(epoc::fs::drive_info_v1 *info, eka2l1::drive *io_drive, const epoc::version fs_ver);

    std::unique_ptr<epoc::fs::drive_info_v1> get_drive_info_struct(const epoc::version fs_version, std::uint32_t &struct_size) {
        if (fs_version.major >= 2) {
            struct_size = sizeof(epoc::fs::drive_info_v2);
            return std::make_unique<epoc::fs::drive_info_v2>();
        }

        struct_size = sizeof(epoc::fs::drive_info_v1);
        return std::make_unique<epoc::fs::drive_info_v1>();
    }

    void fs_server_client::file_drive(service::ipc_context *ctx) {
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

        drive_number drv = static_cast<drive_number>(std::towlower(f->file_name()[0]) - 'a');

        std::unique_ptr<epoc::fs::drive_info_v1> info;
        std::uint32_t info_variant_size = 0;

        info = std::move(get_drive_info_struct(client_version(), info_variant_size));

        std::optional<eka2l1::drive> io_drive = ctx->sys->get_io_system()->get_drive_entry(
            static_cast<drive_number>(drv));

        fill_drive_info(info.get(), io_drive.has_value() ? &io_drive.value() : nullptr, client_version());

        ctx->write_data_to_descriptor_argument<drive_number>(0, drv);
        ctx->write_data_to_descriptor_argument(1, reinterpret_cast<std::uint8_t *>(info.get()), info_variant_size, nullptr, true);

        ctx->complete(epoc::error_none);
    }

    void fill_drive_info(epoc::fs::drive_info_v1 *info, eka2l1::drive *io_drive, const epoc::version ver) {
        info->drive_att = 0;
        info->media_att = 0;

        if (!io_drive || (io_drive->media_type == drive_media::none)) {
            info->type = epoc::fs::media_unknown;
            return;
        }

        switch (io_drive->media_type) {
        case drive_media::physical: {
            info->type = epoc::fs::media_hard_disk;
            info->drive_att = epoc::fs::drive_att_local;

            break;
        }

        case drive_media::rom: {
            info->type = epoc::fs::media_rom;
            info->drive_att = epoc::fs::drive_att_rom;

            break;
        }

        case drive_media::reflect: {
            info->type = epoc::fs::media_rotating;
            info->drive_att = epoc::fs::drive_att_redirected;

            break;
        }

        default:
            break;
        }

        info->battery = epoc::fs::battery_state_not_supported;

        if (static_cast<int>(io_drive->attribute & io_attrib_hidden)) {
            info->drive_att |= epoc::fs::drive_att_hidden;
        }

        if (static_cast<int>(io_drive->attribute & io_attrib_internal)) {
            info->drive_att |= epoc::fs::drive_att_internal;
        }

        if (static_cast<int>(io_drive->attribute & io_attrib_removeable)) {
            info->drive_att |= epoc::fs::drive_att_removable;
        }

        if (static_cast<int>(io_drive->attribute & io_attrib_write_protected)) {
            info->media_att |= epoc::fs::media_att_write_protected;
        }

        if (ver.major >= 2) {
            reinterpret_cast<epoc::fs::drive_info_v2 *>(info)->connection_bus_type = epoc::fs::connection_bus_internal;
        }
    }

    void fs_server_client::drive(service::ipc_context *ctx) {
        drive_number drv = static_cast<drive_number>(*ctx->get_argument_value<std::int32_t>(1));

        std::optional<eka2l1::drive> io_drive = ctx->sys->get_io_system()->get_drive_entry(
            static_cast<drive_number>(drv));

        std::unique_ptr<epoc::fs::drive_info_v1> info;
        std::uint32_t info_variant_size = 0;

        info = std::move(get_drive_info_struct(client_version(), info_variant_size));
        fill_drive_info(info.get(), io_drive.has_value() ? &io_drive.value() : nullptr, client_version());

        ctx->write_data_to_descriptor_argument(0, reinterpret_cast<std::uint8_t *>(info.get()), info_variant_size,
            nullptr, true);

        ctx->complete(epoc::error_none);
    }

    void fs_server_client::drive_list(service::ipc_context *ctx) {
        kernel_system *kern = ctx->sys->get_kernel_system();
        std::optional<std::int32_t> flags = ctx->get_argument_value<std::int32_t>(1);

        if (!kern->is_eka1() && !flags) {
            ctx->complete(epoc::error_argument);
            return;
        }

        std::vector<io_attrib> exclude_attribs;
        std::vector<io_attrib> include_attribs;

        if (!kern->is_eka1()) {
            if (*flags & epoc::fs::drive_att_hidden) {
                if (*flags & epoc::fs::drive_att_exclude) {
                    exclude_attribs.push_back(io_attrib_hidden);
                } else {
                    include_attribs.push_back(io_attrib_hidden);
                }
            }
        }

        std::array<char, drive_count> dlist;

        std::fill(dlist.begin(), dlist.end(), 0);

        for (size_t i = drive_a; i < drive_count; i += 1) {
            auto drv_op = ctx->sys->get_io_system()->get_drive_entry(
                static_cast<drive_number>(i));

            if (drv_op) {
                eka2l1::drive drv = std::move(*drv_op);

                bool out = false;

                for (const auto &exclude : exclude_attribs) {
                    if (static_cast<int>(exclude) & static_cast<int>(drv.attribute)) {
                        dlist[i] = 0;
                        out = true;

                        break;
                    }
                }

                if (!out) {
                    if (include_attribs.empty()) {
                        if (drv.media_type != drive_media::none) {
                            dlist[i] = 1;
                        }

                        continue;
                    }

                    auto meet_one_condition = std::find_if(include_attribs.begin(), include_attribs.end(),
                        [=](io_attrib attrib) { return static_cast<int>(attrib) & static_cast<int>(drv.attribute); });

                    if (meet_one_condition != include_attribs.end()) {
                        dlist[i] = 1;
                    }
                }
            }
        }

        bool success = ctx->write_data_to_descriptor_argument(0, reinterpret_cast<uint8_t *>(&dlist[0]),
            static_cast<std::uint32_t>(dlist.size()));

        if (!success) {
            ctx->complete(epoc::error_argument);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    void fs_server_client::volume(service::ipc_context *ctx) {
        drive_number drv = static_cast<drive_number>(*ctx->get_argument_value<std::int32_t>(1));

        if (static_cast<std::uint32_t>(drv) == DEFAULT_DRIVE_NUM) {
            drv = static_cast<drive_number>(server<fs_server>()->system_drive_prop->get_int());
        }

        std::optional<eka2l1::drive> io_drive = ctx->sys->get_io_system()->get_drive_entry(static_cast<drive_number>(drv));
        std::u16string drive_name = u"EKA2L1_A";

        drive_name.back() += static_cast<char>(drv - drive_a);

#define VOLUME_INFO_GETTERS(info_name)                                                \
    LOG_WARN(SERVICE_EFSRV, "Volume size stubbed with 1GB");                          \
    fill_drive_info(reinterpret_cast<epoc::fs::drive_info_v1 *>(&info_name.drv_info), \
        io_drive.has_value() ? &io_drive.value() : nullptr, cli_ver);                 \
    info_name.uid = drv;                                                              \
    info_name.size = common::GB(1);                                                   \
    info_name.free = common::GB(1);                                                   \
    info_name.name.assign(nullptr, drive_name);

        const epoc::version cli_ver = client_version();
        if (cli_ver.major >= 2) {
            epoc::fs::volume_info_v2 info_new;
            VOLUME_INFO_GETTERS(info_new);

            ctx->write_data_to_descriptor_argument<epoc::fs::volume_info_v2>(0, info_new, nullptr, true);
        } else {
            epoc::fs::volume_info_v1 info_old;
            VOLUME_INFO_GETTERS(info_old);

            ctx->write_data_to_descriptor_argument<epoc::fs::volume_info_v1>(0, info_old, nullptr, true);
        }

        ctx->complete(epoc::error_none);
    }
}