#pragma once

#include <services/socket/common.h>
#include <services/socket/connection.h>
#include <services/socket/host.h>
#include <services/socket/protocol.h>

#include <common/container.h>
#include <kernel/server.h>
#include <services/framework.h>
#include <utils/des.h>

#include <memory>

namespace eka2l1 {
    namespace epoc::socket {
        class socket_connection_proxy : public socket_subsession {
        private:
            connection *conn_;
            bool progress_reported_;

        public:
            explicit socket_connection_proxy(socket_client_session *parent, connection *conn);
            connection *get_connection() const { return conn_; }
            void dispatch(service::ipc_context *ctx) override;
            socket_subsession_type type() const override { return socket_subsession_type_connection; }
        };

        class socket_host_resolver : public socket_subsession {
            std::unique_ptr<host_resolver> resolver_;
            connection *conn_;

        public:
            explicit socket_host_resolver(socket_client_session *parent, std::unique_ptr<host_resolver> &resolver, connection *conn = nullptr);
            void dispatch(service::ipc_context *ctx) override;
            socket_subsession_type type() const override { return socket_subsession_type_host_resolver; }
        };
    }

    enum socket_opcode {
        socket_pr_find = 0x02,
        socket_hr_open = 0x28,
        socket_hr_open_with_connection = 0x38,
        socket_cn_open_with_cn_type = 0x3F,
        socket_cn_get_long_des_setting = 0x51,
        socket_cm_api_ext_interface_send_receive = 0x3F0
    };

    enum socket_opcode_reform {
        socket_reform_hr_open = 0x37,
        socket_reform_hr_open_with_connection = 0x47,
        socket_reform_cn_open_with_cn_type = 0x48,
        socket_reform_cn_get_long_des_setting = 0x51,
        socket_reform_pr_find = 0x82
    };

    enum socket_opcode_old {
        socket_old_pr_find = 0x02,
        socket_old_hr_open = 0x24
    };

    struct protocol_description {
        epoc::buf_static<char16_t, 32> name_;
        std::uint32_t addr_fam_;
        std::uint32_t sock_type_;
        std::uint32_t protocol_;
        epoc::version ver_;
        epoc::socket::byte_order bord_;
        std::uint32_t service_info_;
        std::uint32_t naming_services_;
        std::uint32_t service_sec_;
        std::uint32_t message_size_;
    };

    std::string get_socket_server_name_by_epocver(const epocver ver);

    class socket_server : public service::typical_server {
        std::vector<std::unique_ptr<epoc::socket::protocol>> protocols_;

    public:
        explicit socket_server(eka2l1::system *sys);
        void connect(service::ipc_context &context) override;

        epoc::socket::protocol *find_protocol(const std::uint32_t addr_family, const std::uint32_t protocol_id);
        epoc::socket::protocol *find_protocol_by_name(const std::u16string &name);
        bool add_protocol(std::unique_ptr<epoc::socket::protocol> &pr);
    };

    using socket_subsession_instance = std::unique_ptr<epoc::socket::socket_subsession>;

    struct socket_client_session : public service::typical_session {
    private:
        friend class epoc::socket::socket_host_resolver;
        common::identity_container<socket_subsession_instance> subsessions_;

    public:
        explicit socket_client_session(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);
        bool is_oldarch();
        void fetch(service::ipc_context *ctx) override;

        void hr_create(service::ipc_context *ctx, const bool with_conn);
        void pr_find(service::ipc_context *ctx);
        void cn_open(eka2l1::service::ipc_context *ctx);
        void cn_get_long_des_setting(eka2l1::service::ipc_context *ctx);
    };
}