#include <common/cvt.h>
#include <services/internet/protocols/overall.h>
#include <services/sms/protocols/overall.h>
#include <services/socket/connection.h>
#include <services/socket/host.h>
#include <services/socket/server.h>
#include <services/socket/socket.h>
#include <system/epoc.h>

#include <utils/err.h>

namespace eka2l1 {
    std::string get_socket_server_name_by_epocver(const epocver ver) {
        if (ver <= epocver::eka2) {
            return "SocketServer";
        }
        return "!SocketServer";
    }

    socket_server::socket_server(eka2l1::system *sys)
        : service::typical_server(sys, get_socket_server_name_by_epocver((sys->get_symbian_version_use()))) {
        epoc::internet::add_internet_stack_protocols(this, kern->is_eka1());
        epoc::sms::add_sms_stack_protocols(this, kern->is_eka1());
    }

    void socket_server::connect(service::ipc_context &context) {
        create_session<socket_client_session>(&context);
        context.complete(epoc::error_none);
    }

    epoc::socket::protocol *socket_server::find_protocol(const std::uint32_t addr_family, const std::uint32_t protocol_id) {
        for (auto &pr : protocols_) {
            std::vector<std::uint32_t> ids = pr->family_ids();
            if (std::find(ids.begin(), ids.end(), addr_family) != ids.end()) {
                ids = pr->supported_ids();
                if (std::find(ids.begin(), ids.end(), protocol_id) != ids.end()) {
                    return pr.get();
                }
            }
        }
        return nullptr;
    }

    epoc::socket::protocol *socket_server::find_protocol_by_name(const std::u16string &name) {
        for (auto &pr : protocols_) {
            if (common::compare_ignore_case(pr->name(), name) == 0) {
                return pr.get();
            }
        }
        return nullptr;
    }

    bool socket_server::add_protocol(std::unique_ptr<epoc::socket::protocol> &pr) {
        protocols_.push_back(std::move(pr));
        return true;
    }

    socket_client_session::socket_client_session(service::typical_server *serv, const kernel::uid ss_id,
        epoc::version client_version)
        : service::typical_session(serv, ss_id, client_version) {
    }

    bool socket_client_session::is_oldarch() {
        return server<socket_server>()->get_kernel_object_owner()->get_epoc_version() < epocver::epoc81a;
    }

    void socket_client_session::fetch(service::ipc_context *ctx) {
        if (is_oldarch()) {
            switch (ctx->msg->function) {
            case socket_old_pr_find:
                pr_find(ctx);
                return;
            case socket_old_hr_open:
                hr_create(ctx, false);
                return;
            default:
                break;
            }
        } else {
            if (ctx->sys->get_symbian_version_use() >= epocver::epoc95) {
                switch (ctx->msg->function) {
                case socket_reform_pr_find:
                    pr_find(ctx);
                    return;
                case socket_reform_hr_open:
                    hr_create(ctx, false);
                    return;
                case socket_reform_hr_open_with_connection:
                    hr_create(ctx, true);
                    return;
                case socket_reform_cn_open_with_cn_type:
                    cn_open(ctx);
                    return;
                case socket_reform_cn_get_long_des_setting:
                    cn_get_long_des_setting(ctx);
                    return;
                default:
                    break;
                }
            } else {
                switch (ctx->msg->function) {
                case socket_pr_find:
                    pr_find(ctx);
                    return;
                case socket_hr_open:
                    hr_create(ctx, false);
                    return;
                case socket_hr_open_with_connection:
                    hr_create(ctx, true);
                    return;
                case socket_cn_open_with_cn_type:
                    cn_open(ctx);
                    return;
                case socket_cn_get_long_des_setting:
                    cn_get_long_des_setting(ctx);
                    return;
                default:
                    break;
                }
            }
        }

        std::optional<std::uint32_t> subsess_id = ctx->get_argument_value<std::uint32_t>(3);
        if (subsess_id && (subsess_id.value() > 0)) {
            socket_subsession_instance *inst = subsessions_.get(subsess_id.value());
            if (inst) {
                inst->get()->dispatch(ctx);
                return;
            }
        }

        LOG_ERROR(SERVICE_ESOCK, "Unimplemented opcode for Socket server 0x{:X}", ctx->msg->function);
    }

    void socket_client_session::cn_get_long_des_setting(eka2l1::service::ipc_context *ctx) {
        LOG_TRACE(SERVICE_ESOCK, "CnGetLongDesSetting stubbed");
        ctx->complete(epoc::error_none);
    }

    static void fill_protocol_description(epoc::socket::protocol *pr, protocol_description &des) {
        des.addr_fam_ = pr->family_ids()[0];
        des.protocol_ = pr->supported_ids()[0];
        des.ver_ = pr->ver();
        des.bord_ = pr->get_byte_order();
        des.message_size_ = pr->message_size();
        des.name_.assign(nullptr, pr->name());
        des.service_info_ = 0;
        des.naming_services_ = 0;
        des.service_sec_ = 0;
    }

    void socket_client_session::pr_find(service::ipc_context *ctx) {
        std::optional<std::u16string> protocol_name = ctx->get_argument_value<std::u16string>(1);
        if (!protocol_name.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::socket::protocol *result_pr = server<socket_server>()->find_protocol_by_name(protocol_name.value());
        if (!result_pr) {
            LOG_WARN(SERVICE_ESOCK, "Can't find protocol named {}", common::ucs2_to_utf8(protocol_name.value()));
            ctx->complete(epoc::error_not_found);
            return;
        }

        protocol_description description_to_return;
        fill_protocol_description(result_pr, description_to_return);

        ctx->write_data_to_descriptor_argument<protocol_description>(0, description_to_return);
        ctx->complete(epoc::error_none);
    }

    void socket_client_session::hr_create(service::ipc_context *ctx, const bool with_conn) {
        std::optional<std::uint32_t> addr_family = ctx->get_argument_value<std::uint32_t>(0);
        std::optional<std::uint32_t> protocol = ctx->get_argument_value<std::uint32_t>(1);
        std::optional<std::uint32_t> conn_subhandle = ctx->get_argument_value<std::uint32_t>(2);

        if (!addr_family || !protocol || (with_conn && !conn_subhandle)) {
            ctx->complete(epoc::error_argument);
            return;
        }

        epoc::socket::protocol *target_pr = server<socket_server>()->find_protocol(addr_family.value(), protocol.value());
        if (!target_pr) {
            LOG_ERROR(SERVICE_ESOCK, "Unable to find host resolver with address={}, protocol={}", addr_family.value(), protocol.value());
            ctx->complete(epoc::error_not_found);
            return;
        }

        epoc::socket::socket_connection_proxy *conn = nullptr;
        if (with_conn) {
            socket_subsession_instance *inst = subsessions_.get(conn_subhandle.value());
            if (!inst) {
                ctx->complete(epoc::error_argument);
                return;
            }
            conn = reinterpret_cast<epoc::socket::socket_connection_proxy *>(inst->get());
        }

        std::unique_ptr<epoc::socket::host_resolver> resolver_impl = target_pr->make_host_resolver(addr_family.value(), protocol.value());
        if (!resolver_impl) {
            LOG_ERROR(SERVICE_ESOCK, "The protocol {} does not support resolving host!", common::ucs2_to_utf8(target_pr->name()));
            ctx->complete(epoc::error_not_supported);
            return;
        }

        socket_subsession_instance hr_inst = std::make_unique<epoc::socket::socket_host_resolver>(
            this, resolver_impl, conn ? conn->get_connection() : nullptr);

        const std::uint32_t id = static_cast<std::uint32_t>(subsessions_.add(hr_inst));
        subsessions_.get(id)->get()->set_id(id);

        ctx->write_data_to_descriptor_argument<std::uint32_t>(3, id);
        ctx->complete(epoc::error_none);
    }

    void socket_client_session::cn_open(eka2l1::service::ipc_context *ctx) {
        socket_subsession_instance cn_inst = std::make_unique<epoc::socket::socket_connection_proxy>(this, nullptr);

        const std::uint32_t id = static_cast<std::uint32_t>(subsessions_.add(cn_inst));
        subsessions_.get(id)->get()->set_id(id);

        ctx->write_data_to_descriptor_argument<std::uint32_t>(3, id);
        ctx->complete(epoc::error_none);
    }
}