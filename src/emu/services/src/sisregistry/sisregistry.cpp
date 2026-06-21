/*
 * Copyright (c) 2020 EKA2L1 Team
 *
 * This file is part of EKA2L1 project
 * (see bentokun.github.com/EKA2L1).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <common/chunkyseri.h>
#include <common/cvt.h>
#include <common/time.h>
#include <services/sisregistry/sisregistry.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1 {

    sisregistry_server::sisregistry_server(eka2l1::system *sys)
        : service::typical_server(sys, "!SisRegistryServer") {
    }

    void sisregistry_server::connect(service::ipc_context &context) {
        create_session<sisregistry_client_session>(&context);
        context.complete(epoc::error_none);
    }

    sisregistry_client_session::sisregistry_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    class system_impl; // Dummy reference helper

    // Stub out internal package manager lookups safely:
    manager::packages *sisregistry_client_subsession::package_manager(service::ipc_context *ctx) {
        return nullptr;
    }

    package::object *sisregistry_client_subsession::package_object(service::ipc_context *ctx) {
        return nullptr;
    }

    sisregistry_client_subsession::sisregistry_client_subsession(const epoc::uid package_uid, const std::int32_t index)
        : package_uid_(package_uid)
        , index_(index) {
    }

    void sisregistry_client_session::fetch(service::ipc_context *ctx) {
        if (ctx->sys->get_symbian_version_use() < epocver::epoc95) {
            if ((ctx->msg->function >= sisregistry_get_matching_supported_languages) && (ctx->msg->function <= sisregistry_separator_minimum_read_user_data)) {
                ctx->msg->function++;
                if (ctx->sys->get_symbian_version_use() == epocver::epoc93fp1) {
                    ctx->msg->function++;
                }
            }
        }

        switch (ctx->msg->function) {
        case sisregistry_open_registry_uid: {
            open_registry_uid(ctx);
            break;
        }
        case sisregistry_open_registry_package: {
            open_registry_by_package(ctx);
            break;
        }
        case sisregistry_installed_uid: {
            is_installed_uid(ctx);
            break;
        }
        case sisregistry_installed_uids: {
            installed_uids(ctx);
            break;
        }
        case sisregistry_installed_packages: {
            installed_packages(ctx);
            break;
        }
        case sisregistry_add_entry: {
            add_entry(ctx);
            break;
        }
        case sisregistry_delete_entry: {
            delete_entry(ctx);
            break;
        }
        case sisregistry_package_exists_in_rom: {
            package_exists_in_rom(ctx);
            break;
        }
        case sisregistry_sid_to_package: {
            sid_to_package(ctx);
            break;
        }
        case sisregistry_get_entry: {
            get_entry(ctx);
            break;
        }
        case sisregistry_update_entry: {
            update_entry(ctx);
            break;
        }
        default: {
            std::optional<std::uint32_t> handle = ctx->get_argument_value<std::uint32_t>(3);
            if (handle.has_value()) {
                sisregistry_client_subsession_inst *inst = subsessions_.get(handle.value());
                if (inst) {
                    if ((*inst)->fetch(ctx)) {
                        subsessions_.remove(handle.value());
                    }
                    break;
                }
            }
            ctx->complete(epoc::error_none);
            break;
        }
        }
    }

    bool sisregistry_client_subsession::fetch(service::ipc_context *ctx) {
        ctx->complete(epoc::error_none);
        return true;
    }

    void sisregistry_client_session::open_registry_uid(eka2l1::service::ipc_context *ctx) {
        ctx->complete(epoc::error_not_found);
    }

    void sisregistry_client_session::open_registry_by_package(eka2l1::service::ipc_context *ctx) {
        ctx->complete(epoc::error_not_found);
    }

    void sisregistry_client_session::installed_uids(eka2l1::service::ipc_context *ctx) {
        std::uint32_t count = 0;
        ctx->write_data_to_descriptor_argument(0, count);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::installed_packages(eka2l1::service::ipc_context *ctx) {
        std::uint32_t count = 0;
        ctx->write_data_to_descriptor_argument(0, count);
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::is_installed_uid(eka2l1::service::ipc_context *ctx) {
        ctx->write_data_to_descriptor_argument(1, static_cast<std::int32_t>(0));
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::add_entry(eka2l1::service::ipc_context *ctx) {
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::delete_entry(eka2l1::service::ipc_context *ctx) {
        ctx->complete(epoc::error_none);
    }

    void sisregistry_client_session::package_exists_in_rom(eka2l1::service::ipc_context *ctx) {
        ctx->complete(epoc::error_not_found);
    }

    void sisregistry_client_session::sid_to_package(eka2l1::service::ipc_context *ctx) {
        ctx->complete(epoc::error_not_found);
    }

    void sisregistry_client_session::get_entry(eka2l1::service::ipc_context *ctx) {
        ctx->complete(epoc::error_not_found);
    }

    void sisregistry_client_session::update_entry(eka2l1::service::ipc_context *ctx) {
        ctx->complete(epoc::error_none);
    }

    // Dummy Subsession Stub Implementations
    void sisregistry_client_subsession::close_registry(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::get_version(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::is_in_rom(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::get_selected_drive(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::get_trust_timestamp(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::get_trust_status(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::request_uid(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::request_files(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::request_file_descriptions(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::request_package_augmentations(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::is_preinstalled(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::get_package(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::is_non_removable(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::is_signed_by_sucert(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::request_sids(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::request_size(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::request_package_name(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::request_vendor_localized_name(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::request_embedded_packages(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::request_dependent_packages(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::install_type(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::request_dependencies(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::is_deletable_preinstalled(eka2l1::service::ipc_context *ctx) {}
    void sisregistry_client_subsession::shutdown_all_apps(eka2l1::service::ipc_context *ctx) {}
}