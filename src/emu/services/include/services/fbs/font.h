#pragma once

#include <common/map.h>

#include <mem/ptr.h>
#include <utils/des.h>

#include <vector>

namespace eka2l1 {
    class fbs_server;
    struct fbscli;
}

namespace eka2l1::epoc {
    enum font_posture {
        font_posture_normal = 0,
        font_posture_italic = 1
    };

    enum font_stroke_weight {
        font_stroke_weight_normal = 0,
        font_stroke_weight_bold = 1
    };

    struct open_font_metrics {
        std::int16_t design_height;
        std::int16_t ascent;
        std::int16_t descent;
        std::int16_t max_height;
        std::int16_t max_depth;
        std::int16_t max_width;
        std::int16_t baseline_correction;
        std::int16_t reserved;
    };

    static_assert(sizeof(open_font_metrics) == 16);

    struct open_font_shaping_parameter {
        std::uint32_t text_range_[2];
        std::uint32_t script_code_;
        std::uint32_t language_code_;
    };

    static_assert(sizeof(open_font_shaping_parameter) == 16);

    struct open_font_shaping_header {
        std::uint32_t glyph_count_;
        std::uint32_t char_count_;
        std::int32_t reserved[2];
    };

    static_assert(sizeof(open_font_shaping_header) == 16);

    struct open_font_face_attrib {
        enum {
            bold = 0x1,
            italic = 0x2,
            serif = 0x4,
            mono_width = 0x8,
            symbol = 0x10
        };

        bufc_static<char16_t, 32> name;
        std::uint32_t coverage[4];
        std::int32_t style;
        std::int32_t reserved;
        bufc_static<char16_t, 32> fam_name;
        bufc_static<char16_t, 32> local_full_name;
        bufc_static<char16_t, 32> local_full_fam_name;
        std::int32_t min_size_in_pixels;
        std::int32_t reserved2;
    };

    enum glyph_bitmap_type : std::int16_t {
        default_glyph_bitmap,
        monochrome_glyph_bitmap,
        antialised_glyph_bitmap,
        subpixel_glyph_bitmap,
        four_color_blend_glyph_bitmap,
        undefined_glyph_bitmap,
        antialised_or_monochrome_glyph_bitmap
    };

    struct typeface_info {
        epoc::bufc_static<char16_t, 0x18> name;
        std::uint32_t flags;

        enum {
            tf_propotional = 1,
            tf_serif = 2,
            tf_symbol = 4
        };
    };

    static_assert(sizeof(typeface_info) == 56);

    struct typeface_support {
        typeface_info info_;
        std::uint32_t num_heights_;
        std::int32_t min_height_in_twips_;
        std::int32_t max_height_in_twips_;
        std::int32_t is_scalable_;
    };

    static_assert(sizeof(typeface_support) == 72);

    struct font_style_base {
        enum {
            italic = 0x1,
            bold = 0x2,
            super = 0x4,
            sub = 0x8
        };

        std::uint32_t flags;

        void reset_flags() {
            flags = 0;
        }

        glyph_bitmap_type get_glyph_bitmap_type() const {
            return static_cast<glyph_bitmap_type>(flags >> 16);
        }

        void set_glyph_bitmap_type(const glyph_bitmap_type bmp_type) {
            flags &= 0xFFFF;
            flags |= (static_cast<std::uint16_t>(bmp_type) << 16);
        }
    };

    struct font_style_v1 : public font_style_base {
    };

    struct font_style_v2 : public font_style_base {
        eka2l1::ptr<void> reserved1;
        eka2l1::ptr<void> reserved2;
    };

    struct font_spec_base {
        typeface_info tf;
        std::int32_t height;
    };

    struct font_spec_v1 : public font_spec_base {
        font_style_v1 style;
    };

    struct font_spec_v2 : public font_spec_base {
        font_style_v2 style;
    };

    enum {
        DEAD_VTABLE = 0xDEAD1AB,
        DEAD_ALLOC = 0xDEADA11C
    };

    struct alg_style {
        int baseline_offsets_in_pixel;

        enum {
            bold = 0x1,
            italic = 0x2,
            mono = 0x4
        };

        std::uint8_t flags;
        std::uint8_t width_factor;
        std::uint8_t height_factor;
    };

    struct bitmapfont_base {
        eka2l1::ptr<void> vtable;
    };

    struct bitmapfont_v1 : public bitmapfont_base {
        font_spec_v1 spec_in_twips;
        alg_style algorithic_style;

        eka2l1::ptr<void> allocator;
        int fontbitmap_offset;

        eka2l1::ptr<void> openfont;

        std::uint32_t reserved;
        std::uint32_t font_uid;
    };

    struct bitmapfont_v2 : public bitmapfont_base {
        font_spec_v2 spec_in_twips;
        alg_style algorithic_style;

        eka2l1::ptr<void> allocator;
        int fontbitmap_offset;

        eka2l1::ptr<void> openfont;

        std::uint32_t reserved;
        std::uint32_t font_uid;
    };

    struct open_font_base {
        eka2l1::ptr<void> vtable;
        eka2l1::ptr<void> allocator;

        open_font_metrics metrics;
        eka2l1::ptr<void> sharper;
    };

    struct open_font_v1 : public open_font_base {
        std::int32_t file_offset;
        std::int32_t face_index_offset;
        std::int32_t glyph_cache_offset;
        std::int32_t session_cache_list_offset;

        eka2l1::ptr<void> reserved;
    };

    struct open_font_v2 : public open_font_base {
        std::int32_t font_captial_offset;
        std::int32_t font_max_ascent;
        std::int32_t font_standard_descent;
        std::int32_t font_max_descent;
        std::int32_t font_line_gap;

        std::int32_t file_offset;
        std::int32_t face_index_offset;
        std::int32_t glyph_cache_offset;
        std::int32_t session_cache_list_offset;

        eka2l1::ptr<void> reserved;
    };

    struct open_font_character_metric {
        std::int16_t width;
        std::int16_t height;
        std::int16_t horizontal_bearing_x;
        std::int16_t horizontal_bearing_y;
        std::int16_t horizontal_advance;
        std::int16_t vertical_bearing_x;
        std::int16_t vertical_bearing_y;
        std::int16_t vertical_advance;
        glyph_bitmap_type bitmap_type;
        std::int16_t reserved;
    };

    static_assert(sizeof(open_font_character_metric) == 20);

    struct open_font_glyph_v3 {
        std::int32_t codepoint;
        std::int32_t glyph_index;
        open_font_character_metric metric;
        std::int32_t offset;

        void destroy(fbscli *cli);
    };

    struct open_font_session_cache_entry_v3 : public open_font_glyph_v3 {
        std::int32_t font_offset;
    };

    struct open_font_glyph_v2 {
        std::int32_t codepoint;
        std::int32_t glyph_index;
        open_font_character_metric metric;
        std::int32_t offset;
        std::uint32_t metric_offset;

        void destroy(fbscli *cli);
    };

    struct open_font_glyph_v1 : public open_font_glyph_v3 {
    };

    struct open_font_glyph_v1_use_for_fbs : public open_font_glyph_v2 {
    };

    struct open_font_glyph_cache_entry_v1 : public open_font_glyph_v1 {
        eka2l1::ptr<open_font_glyph_cache_entry_v1> prev_;
        eka2l1::ptr<open_font_glyph_cache_entry_v1> next_;
    };

    struct open_font_glyph_cache_v1 {
        eka2l1::ptr<open_font_glyph_cache_entry_v1> entry_;
        std::uint32_t unk4_;
        std::uint32_t unk8_;

        explicit open_font_glyph_cache_v1();
    };

    struct open_font_session_cache_entry_v2 : public open_font_glyph_v2 {
        std::int32_t font_offset;
        std::int32_t last_use;
    };

    struct open_font_session_cache_entry_v1 : public open_font_glyph_v1 {
        std::int32_t font_offset;
        std::int32_t last_use;
    };

    enum {
        NORMAL_SESSION_CACHE_ENTRY_COUNT = 512,
        SESSION_CACHE_LIST_SESSION_COUNT = 256
    };

    struct open_font_glyph_offset_array {
        std::int32_t offset_array_offset;

        std::int32_t offset_array_count;

        void init(fbscli *cli, const std::int32_t count);
        bool is_entry_empty(fbscli *cli, std::int32_t idx);
        std::int32_t *pointer(fbscli *cli);
        bool set_glyph(fbscli *client, const std::int32_t idx, void *cache_entry);
        void *get_glyph(fbscli *client, const std::int32_t idx);
    };

    struct open_font_session_cache_base {
        std::int32_t session_handle;
    };

    struct open_font_session_cache_old : public open_font_session_cache_base {
        open_font_glyph_offset_array offset_array;
        std::uint32_t last_use_counter{ 0 };

        template <typename T>
        void add_glyph(fbscli *cli, const std::uint32_t code, void *the_glyph);

        template <typename T>
        void destroy(fbscli *cli);
    };

    struct open_font_session_cache_v3 : public open_font_session_cache_base {
        std::int32_t padding_;
        std::int64_t random_seed;

        open_font_glyph_offset_array offset_array;

        void add_glyph(fbscli *cli, const std::uint32_t code, void *the_glyph);
        void destroy(fbscli *cli);
    };

    static_assert(sizeof(open_font_session_cache_v3) == 24);

    struct open_font_session_cache_link {
        eka2l1::ptr<open_font_session_cache_link> next{ 0 };
        eka2l1::ptr<open_font_session_cache_old> cache{ 0 };

        open_font_session_cache_link *get_or_create(fbscli *cli);
        bool remove(fbscli *cli);
    };

    enum {
        NORMAL_SESSION_CACHE_LIST_ENTRY_COUNT = 256
    };

    struct open_font_session_cache_list : public common::vector_static_map<std::int32_t, std::int32_t,
                                              SESSION_CACHE_LIST_SESSION_COUNT> {
        open_font_session_cache_v3 *get(fbscli *cli, const std::int32_t session_handle, const bool create = false);
        bool erase_cache(fbscli *cli);
    };
}