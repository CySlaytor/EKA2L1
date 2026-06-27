#include <common/buffer.h>
#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/log.h>
#include <common/pystr.h>

#include <services/centralrepo/centralrepo.h>
#include <services/ui/cap/consts.h>
#include <services/ui/cap/oom_app.h>
#include <services/window/classes/wingroup.h>
#include <services/window/window.h>
#include <utils/err.h>

#include <kernel/kernel.h>
#include <system/epoc.h>
#include <vfs/vfs.h>

namespace eka2l1 {
    oom_ui_app_server::oom_ui_app_server(eka2l1::system *sys)
        : service::typical_server(sys, OOM_APP_UI_SERVER_NAME) {
    }

    void oom_ui_app_server::connect(service::ipc_context &ctx) {
        create_session<oom_ui_app_session>(&ctx);
        typical_server::connect(ctx);
    }

    epoc::cap::sgc_server *oom_ui_app_server::get_sgc_server() {
        if (!sgc) {
            init(sys->get_kernel_system(), sys->get_io_system(), sys->get_device_manager());
        }
        return sgc.get();
    }

    oom_ui_app_session::oom_ui_app_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_version, const bool is_old_layout)
        : service::typical_session(svr, client_ss_uid, client_version)
        , blank_count(0)
        , old_layout(is_old_layout) {
    }

    void oom_ui_app_session::blank_screen(service::ipc_context *ctx) {
        blank_count++;
        LOG_TRACE(SERVICE_UI, "Blanking screen in AKNCAP session stubbed");
        ctx->complete(epoc::error_none);
    }

    void oom_ui_app_session::unblank_screen(service::ipc_context *ctx) {
        blank_count--;
        LOG_TRACE(SERVICE_UI, "Unblanking screen in AKNCAP session stubbed");
        ctx->complete(epoc::error_none);
    }

    void oom_ui_app_session::fetch(service::ipc_context *ctx) {
        if (ctx->sys->get_symbian_version_use() <= epocver::epoc93fp2) {
            if (ctx->msg->function >= akn_eik_app_ui_move_app_in_z_order) {
                ctx->msg->function++;
            }
        }

        switch (ctx->msg->function) {
        case akn_eik_app_ui_layout_config_size: {
            server<oom_ui_app_server>()->get_layout_config_size(*ctx);
            break;
        }
        case akn_eik_app_ui_get_layout_config: {
            server<oom_ui_app_server>()->get_layout_config(*ctx);
            break;
        }
        case akn_eik_app_ui_set_sgc_params: {
            server<oom_ui_app_server>()->set_sgc_params(*ctx, old_layout);
            break;
        }
        case akns_blank_screen: {
            blank_screen(ctx);
            break;
        }
        case akns_unblank_screen: {
            unblank_screen(ctx);
            break;
        }
        case akns_update_key_block_mode:
            server<oom_ui_app_server>()->update_key_block_mode(*ctx);
            break;
        default: {
            LOG_WARN(SERVICE_UI, "Unimplemented opcode for OOM AKNCAP server: 0x{:X}, fake return with epoc::error_none", ctx->msg->function);
            ctx->complete(epoc::error_none);
        }
        }
    }

    std::uint32_t calculate_screen_style_hash(const std::string &style) {
        std::uint64_t hash = 0;
        static constexpr std::uint8_t HASH_MULT = 131;

        for (const char c : style) {
            hash *= HASH_MULT;
            hash += c;
        }
        return static_cast<std::uint32_t>(hash);
    }

    std::string oom_ui_app_server::get_layout_buf() {
        kernel_system *kern = sys->get_kernel_system();
        if (!winsrv) {
            winsrv = reinterpret_cast<window_server *>(&(*kern->get_by_name<service::server>(
                eka2l1::get_winserv_name_by_epocver(sys->get_symbian_version_use()))));
        }

        epoc::config::screen *scr_config = winsrv->get_current_focus_screen_config();
        assert(scr_config && "Current screen config must be valid");

        akn_layout_config akn_config;
        akn_config.num_screen_mode = static_cast<std::int32_t>(scr_config->modes.size());
        akn_config.num_hardware_mode = static_cast<std::int32_t>(scr_config->hardware_states.size());
        akn_config.screen_modes = sizeof(akn_layout_config);
        akn_config.hardware_infos = sizeof(akn_layout_config) + sizeof(akn_screen_mode_info) * akn_config.num_screen_mode;

        std::string result;
        result.append(reinterpret_cast<char *>(&akn_config), sizeof(akn_layout_config));

        for (std::size_t i = 0; i < scr_config->modes.size(); i++) {
            akn_screen_mode_info mode_info;
            mode_info.loc = akn_softkey_loc::bottom;
            mode_info.mode_num = scr_config->modes[i].mode_number;
            mode_info.dmode = epoc::display_mode::color16ma;
            mode_info.info.orientation = epoc::number_to_orientation(scr_config->modes[i].rotation);
            mode_info.info.pixel_size = scr_config->modes[i].size;
            mode_info.info.twips_size = mode_info.info.pixel_size * epoc::get_approximate_pixel_to_twips_mul(kern->get_epoc_version());
            mode_info.screen_style_hash = calculate_screen_style_hash(scr_config->modes[i].style);

            result.append(reinterpret_cast<char *>(&mode_info), sizeof(akn_screen_mode_info));
        }

        for (std::size_t i = 0; i < scr_config->hardware_states.size(); i++) {
            akn_hardware_info hard_info;
            hard_info.alt_screen_mode = scr_config->hardware_states[i].mode_alternative;
            hard_info.screen_mode = scr_config->hardware_states[i].mode_normal;
            hard_info.state_num = scr_config->hardware_states[i].state_number;
            hard_info.key_mode = scr_config->hardware_states[i].switch_keycode;

            result.append(reinterpret_cast<char *>(&hard_info), sizeof(akn_hardware_info));
        }
        return result;
    }

    void oom_ui_app_server::get_layout_config_size(service::ipc_context &ctx) {
        layout_buf = get_layout_buf();
        int layout_buf_size = static_cast<int>(layout_buf.size());
        ctx.write_data_to_descriptor_argument<int>(0, layout_buf_size);
        ctx.complete(epoc::error_none);
    }

    void oom_ui_app_server::get_layout_config(service::ipc_context &ctx) {
        layout_buf = get_layout_buf();
        ctx.write_data_to_descriptor_argument(0, reinterpret_cast<std::uint8_t *>(&layout_buf[0]), static_cast<std::uint32_t>(layout_buf.size()));
        ctx.complete(epoc::error_none);
    }

    void oom_ui_app_server::set_sgc_params(service::ipc_context &ctx, const bool old_layout) {
        std::optional<epoc::sgc_params> params = ctx.get_argument_data_from_descriptor<epoc::sgc_params>(0);

        if (!params.has_value()) {
            if (old_layout) {
                params = std::make_optional<epoc::sgc_params>();
                params->app_screen_mode = 0;
                params->window_group_id = ctx.get_argument_value<std::int32_t>(0).value();
                params->bit_flags = ctx.get_argument_value<std::uint32_t>(1).value();
                params->sp_layout = ctx.get_argument_value<std::uint32_t>(2).value();
                params->sp_flag = ctx.get_argument_value<std::uint32_t>(3).value();
            } else {
                ctx.complete(epoc::error_argument);
                return;
            }
        }

        get_sgc_server()->change_wg_param(params->window_group_id, *reinterpret_cast<epoc::cap::sgc_server::wg_state::wg_state_flags *>(&(params->bit_flags)), params->sp_layout,
            params->sp_flag, params->app_screen_mode);

        ctx.complete(epoc::error_none);
    }

    void oom_ui_app_server::update_key_block_mode(service::ipc_context &ctx) {
        std::optional<std::uint32_t> disable_it = ctx.get_argument_value<std::uint32_t>(0);
        if (!disable_it.has_value()) {
            ctx.complete(epoc::error_argument);
            return;
        }
        eik->key_block_mode(!static_cast<bool>(disable_it.value()));
        ctx.complete(epoc::error_none);
    }

    void oom_ui_app_server::init(kernel_system *kern, io_system *io, device_manager *mngr) {
        const std::lock_guard<std::mutex> guard(lock);
        auto cenrep = reinterpret_cast<central_repo_server *>(kern->get_by_name<service::server>(CENTRAL_REPO_SERVER_NAME));
        sgc = std::make_unique<epoc::cap::sgc_server>();
        eik = std::make_unique<epoc::cap::eik_server>(kern);

        if (cenrep) {
            coe_storage = std::make_unique<epoc::coe_data_storage>(cenrep, io, mngr);
        }
        get_sgc_server()->init(kern, sys->get_graphics_driver());
        eik->init(kern);
    }

    std::optional<akn_running_app_info> get_akn_app_info_from_window_group(epoc::window_group *group) {
        common::pystr16 the_name = group->name;
        const std::vector<common::pystr16> the_name_parts = the_name.split(u'\0');

        if (the_name_parts.size() >= 3) {
            akn_running_app_info info;
            info.app_name_ = the_name_parts[2].std_str();
            info.app_uid_ = the_name_parts[1].as_int<std::uint32_t>(0, 16);
            info.flags_ = the_name_parts[0].as_int<std::uint32_t>(0, 16);
            info.screen_number_ = group->scr->number;
            info.associated_ = group->client->get_client()->owning_process();

            if (info.app_uid_ != 0) {
                if (group->scr->focus == group) {
                    info.flags_ |= akn_running_app_info::FLAG_CURRENTLY_PLAY;
                }
                return info;
            }
        }
        return std::nullopt;
    }
}