#pragma once

#include <services/allocator.h>
#include <services/fbs/adapter/font_adapter.h>
#include <services/fbs/bitmap.h>
#include <services/fbs/font.h>
#include <services/fbs/font_atlas.h>
#include <services/fbs/font_store.h>
#include <services/framework.h>
#include <services/window/common.h>

#include <common/allocator.h>
#include <common/hash.h>
#include <drivers/graphics/common.h>

#include <kernel/server.h>
#include <loader/mbm.h>
#include <mem/ptr.h>
#include <utils/dll.h>
#include <vfs/vfs.h>

#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <unordered_map>

namespace eka2l1 {
    class fbs_server;
    class io_system;
    struct file;
    struct directory;
    using symfile = std::unique_ptr<file>;

    namespace drivers {
        class graphics_driver;
    }

    namespace epoc {
        constexpr std::uint16_t RETURN_POINTER_NOT_OFFSET_BUILD_LIMIT = 94;

        struct open_font_session_cache_list;
        struct open_font_session_cache_link;

        struct bitmapfont_base;
        struct open_font;
        struct open_font_info;

        bool does_client_use_pointer_instead_of_offset(fbscli *cli);
        std::string get_fbs_server_name_by_epocver(const epocver ver);
        void query_fbs_feature_support(fbs_server *serv, bool &support_current_display_mode, bool &support_dirty_bitmap);
    }

    enum fbs_opcode {
        fbs_init,
        fbs_shutdown,
        fbs_close,
        fbs_resource_count,
        fbs_num_typefaces,
        fbs_typeface_support,
        fbs_font_height_in_twips,
        fbs_font_height_in_pixels,
        fbs_add_font_store_file,
        fbs_install_font_store_file,
        fbs_remove_font_store_file,
        fbs_set_pixel_size_in_twips,
        fbs_get_font_by_id,
        fbs_font_dup,
        fbs_bitmap_create,
        fbs_bitmap_resize,
        fbs_bitmap_dup,
        fbs_bitmap_load,
        fbs_bitmap_default_alloc_fail,
        fbs_bitmap_default_mark,
        fbs_bitmap_default_mark_end,
        fbs_user_alloc_fail,
        fbs_user_mark,
        fbs_user_mark_end,
        fbs_heap_check,
        fbs_rasterize,
        fbs_face_attrib,
        fbs_has_character,
        fbs_set_default_glyph_bitmap_type,
        fbs_get_default_glyph_bitmap_type,
        fbs_font_name_alias,
        fbs_bitmap_compress,
        fbs_get_heap_sizes,
        fbs_nearest_font_design_height_in_twips,
        fbs_nearest_font_max_height_in_twips,
        fbs_nearest_font_design_height_in_pixels,
        fbs_nearest_font_max_height_in_pixels,
        fbs_shape_text,
        fbs_shape_delete,
        fbs_default_lang_for_metrics,
        fbs_set_twips_height,
        fbs_get_twips_height,
        fbs_compress,
        fbs_bitmap_bg_compress,
        fbs_unused1,
        fbs_set_system_default_typeface_name,
        fbs_get_all_bitmap_handles,
        fbs_mess_unused1,
        fbs_set_heap_fail,
        fbs_heap_count,
        fbs_set_heap_reset,
        fbs_set_heap_check,
        fbs_heap,
        fbs_mess_unused2,
        fbs_bitmap_clean,
        fbs_bitmap_load_fast,
        fbs_bitmap_notify_dirty,
        fbs_bitmap_cancel_notify_dirty,
        fbs_register_linked_typeface,
        fbs_fetch_linked_typeface,
        fbs_set_duplicate_fail,
        fbs_update_linked_typeface,
        fbs_get_font_table,
        fbs_release_font_table,
        fbs_get_glyph_outline,
        fbs_release_glyph_outline,
        fbs_get_glyphs,
        fbs_no_op,
        fbs_get_glyph_metrics,
        fbs_atlas_font_count,
        fbs_atlas_glyph_count,
        fbs_oogm_notification,
        fbs_get_glyph_cache_metrics
    };

    enum fbs_legacy_level {
        FBS_LEGACY_LEVEL_MORDEN = 0,
        FBS_LEGACY_LEVEL_EARLY_EKA2 = 1,
        FBS_LEGACY_LEVEL_SYMBIAN_92 = 2,
        FBS_LEGACY_LEVEL_KERNEL_TRANSITION = 3,
        FBS_LEGACY_LEVEL_EARLY_KERNEL_TRANSITION = 4,
        FBS_LEGACY_LEVEL_S60V1 = 5
    };

    enum class fbsobj_kind {
        font,
        bitmap
    };

    struct fbsobj : public epoc::ref_count_object {
        fbsobj_kind kind;

        explicit fbsobj(const fbsobj_kind kind)
            : kind(kind) {
        }
        ~fbsobj() override {}
    };

    struct fbscli;
    struct fbsfont;
    struct fbsbitmap;

    struct fbscli : public service::typical_session {
        service::uid connection_id_{ 0 };
        epoc::notify_info dirty_nof_;

        bool support_dirty_bitmap{ true };
        bool support_current_display_mode{ true };

        epoc::open_font_glyph_v1_use_for_fbs *glyph_info_for_legacy_return_;
        address glyph_info_for_legacy_return_addr_;

        epoc::bitmapfont_base *create_bitmap_open_font(epoc::open_font_info &info, epoc::font_spec_base &spec, kernel::process *font_user);

        template <typename T, typename Q>
        void fill_bitmap_information(T *bitmapfont, Q *of, epoc::open_font_info &info, epoc::font_spec_base &spec, kernel::process *font_user);

        void write_font_handle(service::ipc_context *ctx, fbsfont *font, const int index);

        explicit fbscli(service::typical_server *serv, const kernel::uid ss_id, epoc::version client_version);
        ~fbscli();

        void num_typefaces(service::ipc_context *ctx);
        void typeface_support(service::ipc_context *ctx);
        void get_nearest_font(service::ipc_context *ctx);
        void rasterize_glyph(service::ipc_context *ctx);
        void load_bitmap_fast(service::ipc_context *ctx);
        void duplicate_font(service::ipc_context *ctx);
        void duplicate_bitmap(service::ipc_context *ctx);
        void create_bitmap(service::ipc_context *ctx);
        void resize_bitmap(service::ipc_context *ctx);
        void notify_dirty_bitmap(service::ipc_context *ctx);
        void cancel_notify_dirty_bitmap(service::ipc_context *ctx);
        void get_clean_bitmap(service::ipc_context *ctx);

        fbsbitmap *get_clean_bitmap(fbsbitmap *bmp);
        void load_bitmap_impl(service::ipc_context *ctx, file *source);

        void fetch(service::ipc_context *ctx) override;
        fbsfont *get_font_object(service::ipc_context *ctx);

        void set_default_glyph_bitmap_type(service::ipc_context *ctx);
    };

    struct fbsfont : public fbsobj {
        std::int32_t guest_font_offset;
        epoc::open_font_info of_info;
        fbs_server *serv;

        epoc::font_atlas atlas;
        std::vector<std::uint8_t *> shapings;

        struct tf_table_info {
            std::uint32_t offset;
            std::uint32_t length;
            std::int32_t ref_count = 0;
        };

        std::map<std::uint32_t, tf_table_info> font_tables;

        explicit fbsfont()
            : fbsobj(fbsobj_kind::font) {
        }

        ~fbsfont() override;
    };

    struct fbsbitmap : public fbsobj {
        epoc::bitwise_bitmap *bitmap_;
        fbs_server *serv_;
        bool shared_{ false };
        fbsbitmap *clean_bitmap;
        bool support_dirty_bitmap;
        std::uint32_t reserved_height_each_side_;

        explicit fbsbitmap(fbs_server *srv, epoc::bitwise_bitmap *bitmap, const bool shared,
            const bool support_dirty_bitmap, const std::uint32_t reserved_height_each_size = 0)
            : fbsobj(fbsobj_kind::bitmap)
            , bitmap_(bitmap)
            , serv_(srv)
            , shared_(shared)
            , clean_bitmap(nullptr)
            , support_dirty_bitmap(support_dirty_bitmap)
            , reserved_height_each_side_(reserved_height_each_size) {
        }

        ~fbsbitmap() override;
        fbsbitmap *final_clean();
        std::uint8_t *original_pointer(fbs_server *serv) {
            return bitmap_->data_pointer(serv) - bitmap_->byte_width_ * reserved_height_each_side_;
        }
    };

    struct fbsbitmap_cache_info {
        std::u16string path;
        int bitmap_idx;
    };

    inline bool operator==(const fbsbitmap_cache_info &lhs, const fbsbitmap_cache_info &rhs) {
        return (lhs.path == rhs.path) && (lhs.bitmap_idx == rhs.bitmap_idx);
    }
}

namespace std {
    template <>
    struct hash<eka2l1::fbsbitmap_cache_info> {
        std::size_t operator()(eka2l1::fbsbitmap_cache_info const &info) const noexcept {
            std::size_t seed = 0x151A5151;
            eka2l1::common::hash_combine(seed, info.path);
            eka2l1::common::hash_combine(seed, info.bitmap_idx);
            return seed;
        }
    };
}

namespace eka2l1 {

    enum fbs_load_data_err {
        fbs_load_data_err_none,
        fbs_load_data_err_out_of_mem,
        fbs_load_data_err_invalid_arg,
        fbs_load_data_err_read_decomp_fail,
        fbs_load_data_err_small_bitmap
    };

    struct fbs_bitmap_data_info {
        eka2l1::vec2 size_;
        epoc::display_mode dpm_;
        epoc::bitmap_file_compression comp_;
        std::uint8_t *data_;
        std::size_t data_size_;
        explicit fbs_bitmap_data_info();
    };

    class fbs_server : public service::typical_server {
        friend struct fbscli;
        friend struct fbsfont;

        server_ptr fs_server;
        chunk_ptr shared_chunk;

        std::uint8_t *base_shared_chunk;
        eka2l1::ptr<void> bmp_font_vtab;
        codeseg_ptr fntstr_seg;

        std::u16string default_system_font;
        std::unordered_map<fbsbitmap_cache_info, fbsbitmap *> shared_bitmaps;
        std::unique_ptr<epoc::chunk_allocator> shared_chunk_allocator;

        epoc::open_font_session_cache_list *session_cache_list;
        epoc::open_font_session_cache_link *session_cache_link;
        epoc::font_store persistent_font_store;

        std::atomic<service::uid> connection_id_counter{ 0x1234 };
        service::normal_object_container font_obj_container;

        eka2l1::vec2 pixel_size_in_twips;
        epoc::glyph_bitmap_type default_glyph_bitmap_type;

    protected:
        void load_fonts_from_directory(eka2l1::io_system *io, eka2l1::directory *dir);
        void load_fonts(eka2l1::io_system *io);
        void initialize_server();
        bool add_single_font(eka2l1::io_system *io, const std::u16string &path);

    public:
        explicit fbs_server(eka2l1::system *sys);
        ~fbs_server() override;

        service::uid init();
        void connect(service::ipc_context &context) override;
        bool is_large_bitmap(const std::uint32_t compressed_size) const;

        fbsbitmap *create_bitmap(fbs_bitmap_data_info &info, const bool alloc_data = true, const bool support_current_display_mod_flag = true,
            const bool support_dirty = true);
        bool free_bitmap(fbsbitmap *bmp);
        int legacy_level() const;
        drivers::graphics_driver *get_graphics_driver();
        fbsfont *look_for_font_with_address(const eka2l1::address addr);

        std::uint8_t *get_shared_chunk_base() const { return base_shared_chunk; }

        eka2l1::ptr<std::uint8_t> host_ptr_to_guest_general_data(void *host_ptr) {
            return shared_chunk->base(nullptr) + static_cast<std::uint32_t>(reinterpret_cast<std::uint8_t *>(host_ptr) - base_shared_chunk);
        }

        void *guest_general_data_to_host_ptr(eka2l1::ptr<std::uint8_t> addr) {
            return base_shared_chunk + (addr.ptr_address() - shared_chunk->base(nullptr).ptr_address());
        }

        std::int32_t host_ptr_to_guest_shared_offset(void *host_ptr) {
            return static_cast<std::int32_t>(reinterpret_cast<std::uint8_t *>(host_ptr) - base_shared_chunk);
        }

        template <typename T>
        void destroy_bitmap_font(T *bitmapfont);

        void *load_data_to_rom(loader::mbm_file &mbmf_, const std::size_t idx_, std::size_t &size_decomp, int *err_code);
        void *allocate_general_data_impl(const std::size_t s);
        bool free_general_data_impl(const void *ptr);

        template <typename T, typename... Args>
        T *allocate_general_data(Args... construct_args) {
            return shared_chunk_allocator->allocate_struct<T>(construct_args...);
        }

        fbsfont *get_font(const service::uid id);

        template <typename T>
        bool free_general_data(const T *dat) {
            return free_general_data_impl(dat);
        }

        eka2l1::vec2 get_pixel_size_in_twips() const { return pixel_size_in_twips; }
        void set_pixel_size_in_twips(const eka2l1::vec2 &new_pixel_size_in_twips) { pixel_size_in_twips = new_pixel_size_in_twips; }
        epoc::glyph_bitmap_type get_default_glyph_bitmap_type() const { return default_glyph_bitmap_type; }
        void set_default_glyph_bitmap_type(const epoc::glyph_bitmap_type new_value) { default_glyph_bitmap_type = new_value; }
    };
}

namespace eka2l1::epoc {
    bool does_client_use_pointer_instead_of_offset(fbscli *cli);
    std::string get_fbs_server_name_by_epocver(const epocver ver);
}