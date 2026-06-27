#pragma once

#include <BS_thread_pool.hpp>
#include <functional>
#include <mutex>
#include <services/framework.h>
#include <system/epoc.h>
#include <utils/des.h>
#include <vector>
#include <vfs/vfs.h>

#define MAPPED_EXECUTABLE_HEAD_STRING u"!HostApp"
#define MAPPED_EXECUTABLE_HEAD_STRING_LENGTH 8
#define UNIQUE_MAPPED_EXTENSION_STRING u".exe"
#define UNIQUE_MAPPED_EXTENSION_STRING_LENGTH 4

namespace eka2l1 {
    class io_system;
    class fbs_server;
    class fs_server;

    struct fbsbitmap;

    namespace common {
        class ro_stream;
    }

    namespace kernel {
        class process;
    }

    namespace epoc {
        struct bitwise_bitmap;
    }

    namespace epoc::apa {
        struct command_line;
    }

    struct data_type {
        int priority_;
        std::u16string type_;
    };

    struct view_data {
        std::uint32_t uid_;
        std::uint32_t screen_mode_;
        std::uint16_t icon_count_;
        std::u16string caption_;
        std::u16string icon_path_;
    };

    struct apa_capability {
        enum embeddability {
            not_embeddable = 0,
            embeddable = 1,
            embeddable_only = 2
        };

        enum {
            built_as_dll = 0x10000000,
            non_native = 0x20000000
        };

        std::uint32_t flags = 0;
        bool is_hidden = false;
        embeddability ability = not_embeddable;
        bool support_being_asked_to_create_new_file = false;
        bool launch_in_background = false;
        std::u16string group_name;

        bool internalize(common::ro_stream &stream) {
            return true;
        }
    };

    using file_ownership_list = std::vector<std::u16string>;

    struct apa_app_info {
        std::uint32_t uid;
        epoc::filename app_path;
        epoc::apa_app_caption short_caption;
        epoc::apa_app_caption long_caption;
        explicit apa_app_info() {}
    };

    struct apa_app_icon {
        std::uint16_t number_;
        fbsbitmap *bmp_;
        address bmp_rom_addr_;
    };

    enum apa_legacy_level {
        APA_LEGACY_LEVEL_OLD = -3,
        APA_LEGACY_LEVEL_S60V2 = -2,
        APA_LEGACY_LEVEL_TRANSITION = -1,
        APA_LEGACY_LEVEL_MORDEN = 0
    };

    using apa_app_masked_icon_bitmap = std::pair<epoc::bitwise_bitmap *, epoc::bitwise_bitmap *>;

    struct apa_app_registry {
        apa_app_info mandatory_info;
        apa_capability caps;

        std::u16string rsc_path;
        std::uint64_t last_rsc_modified{ 0 };
        std::u16string localised_info_rsc_path;
        std::uint32_t localised_info_rsc_id{ 1 };

        std::uint8_t default_screen_number{ 0 };

        std::int16_t icon_count;
        std::u16string icon_file_path;

        std::vector<apa_app_icon> app_icons;
        std::vector<data_type> data_types;
        std::vector<view_data> view_datas;
        file_ownership_list ownership_list;

        drive_number land_drive;

        void get_launch_parameter(std::u16string &native_executable_path, epoc::apa::command_line &args);
        bool supports_screen_mode(const int mode_num);
    };

    bool read_registeration_info(common::ro_stream *stream, apa_app_registry &reg, const drive_number land_drive, const bool app_path_oldarch);
    bool read_registeration_info_aif(common::ro_stream *stream, apa_app_registry &reg, const drive_number land_drive, language lang);
    bool read_localised_registration_info(common::ro_stream *stream, apa_app_registry &reg, const drive_number land_drive);

    const std::string get_app_list_server_name_by_epocver(const epocver ver);

    class applist_session : public service::typical_session {
    private:
        enum app_filter_method {
            APP_FILTER_BY_EMBED,
            APP_FILTER_BY_FLAGS,
            APP_FILTER_NONE
        };

        std::size_t current_index_;
        std::uint32_t flags_mask_;
        std::uint32_t flags_match_value_;
        std::uint32_t requested_screen_mode_;

        app_filter_method filter_method_;

    public:
        explicit applist_session(service::typical_server *svr, kernel::uid client_ss_uid, epoc::version client_ver);
        void fetch(service::ipc_context *ctx);
    };

    class applist_server : public service::typical_server {
        friend class applist_session;

        std::vector<apa_app_registry> regs;
        std::unordered_map<epoc::uid, std::u16string> uids_app_to_executable;

        std::uint32_t flags{ 0 };
        std::uint32_t avail_drives_{ 0 };

        std::vector<std::size_t> poll_events_;
        std::size_t drive_change_handle_{ 0 };

        fbs_server *fbsserv;
        fs_server *fsserv;

        BS::thread_pool loading_thread_pool_;

        enum {
            AL_INITED = 0x1
        };

        void sort_registry_list();
        void init();

        bool load_registry(eka2l1::io_system *io, const std::u16string &path, drive_number land_drive, const language ideal_lang = language::en);

        void rescan_registries_on_drive_newarch(eka2l1::io_system *io, const drive_number num, std::vector<std::u16string> &results);
        void rescan_registries_on_drive_newarch_with_path(eka2l1::io_system *io, const drive_number num, const std::u16string &path,
            std::vector<std::u16string> &results);

        void get_app_info(service::ipc_context &ctx);
        void get_app_icon_file_name(service::ipc_context &ctx);
        void get_preferred_buf_size(service::ipc_context &ctx);
        void get_app_for_document(service::ipc_context &ctx);
        void get_app_for_document_impl(service::ipc_context &ctx, const std::u16string &path);

        void connect(service::ipc_context &ctx) override;

    public:
        explicit applist_server(system *sys);
        ~applist_server() override;

        int legacy_level();

        std::mutex list_access_mut_;
        bool rescan_registries(eka2l1::io_system *io);
        apa_app_registry *get_registration(const std::uint32_t uid);

        void add_app_uid_to_host_launch_name(const epoc::uid app_uid, const std::u16string &host_launch_name);
    };
}