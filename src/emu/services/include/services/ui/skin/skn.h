#pragma once

#include <common/buffer.h>
#include <common/rgb.h>
#include <common/types.h>
#include <common/vecx.h>
#include <map>
#include <services/ui/skin/common.h>
#include <unordered_map>

namespace eka2l1::epoc {
    enum akn_skin_descriptor_chunk_type {
        as_desc_skin_desc = 0,
        as_desc_name,
        as_desc_filename,
        as_desc_skin_desc_class,
        as_desc_skin_desc_bmp_item_def,
        as_desc_skin_desc_color_tbl_item_def,
        as_desc_skin_desc_img_tbl_item_def,
        as_desc_skin_desc_img_attribs,
        as_desc_skin_desc_img_bmp_anim,
        as_desc_skin_desc_img_lang_override,
        as_desc_wallpaper,
        as_desc_info,
        as_desc_skin_desc_string_item_def,
        as_desc_release26,
        as_desc_target_dvc,
        as_desc_lang,
        as_desc_effect_queue,
        as_desc_effect,
        as_desc_release_generic,
        as_desc_anim,
        as_desc_anim_param_group
    };

    enum skn_descriptor_offset {
        skn_desc_dfo_common_len = 0,
        skn_desc_dfo_common_type = 4,
        skn_desc_dfo_common_ver = 6,
        skn_desc_dfo_skin_chunks_count = 38,
        skn_desc_dfo_skin_content = 42,
        skn_desc_dfo_name_lang = 8,
        skn_desc_dfo_name_name_Len = 10,
        skn_desc_dfo_name_name = 12,
        skn_desc_dfo_filename_filename_id = 8,
        skn_desc_dfo_filename_len = 12,
        skn_desc_dfo_filename_filename = 14,
        skn_desc_dfo_class_class = 8,
        skn_desc_dfo_class_chunk_n = 9,
        skn_desc_dfo_class_content = 13,
        skn_desc_dfo_bitmap_hash_id = 8,
        skn_desc_dfo_bitmap_filename_id = 16,
        skn_desc_dfo_bitmap_bitmap_idx = 20,
        skn_desc_dfo_bitmap_mask_idx = 24,
        skn_desc_dfo_bitmap_attribs = 28,
        skn_desc_dfo_color_tab_major = 8,
        skn_desc_dfo_color_tab_minor = 12,
        skn_desc_dfo_color_tab_colors_count = 16,
        skn_desc_dfo_color_tab_color_idx0 = 17,
        skn_desc_dfo_color_tab_color_rgb0 = 19,
        skn_desc_dfo_color_tab_color_size = 6,
        skn_desc_dfo_img_table_major = 8,
        skn_desc_dfo_img_table_minor = 12,
        skn_desc_dfo_img_table_images_count = 16,
        skn_desc_dfo_img_table_image_major0 = 17,
        skn_desc_dfo_img_table_image_minor = 21,
        skn_desc_dfo_img_table_image_size = 8,
        skn_desc_dfo_bmp_anim_major = 8,
        skn_desc_dfo_bmp_anim_minor = 12,
        skn_desc_dfo_bmp_anim_interval = 16,
        skn_desc_dfo_bmp_anim_playmode = 18,
        skn_desc_dfo_bmp_anim_flash = 19,
        skn_desc_dfo_bmp_anim_frames_count = 20,
        skn_desc_dfo_bmp_anim_frame_major0 = 21,
        skn_desc_dfo_bmp_anim_frame_minor0 = 25,
        skn_desc_dfo_bmp_anim_frame_time0 = 29,
        skn_desc_dfo_bmp_anim_frame_posx0 = 31,
        skn_desc_dfo_bmp_anim_frame_posy0 = 33,
        skn_desc_dfo_bmp_anim_frame_size = 14,
        skn_desc_dfo_attribs_attrib_flags = 8,
        skn_desc_dfo_attribs_alignment = 9,
        skn_desc_dfo_attribs_coordx = 10,
        skn_desc_dfo_attribs_coordy = 12,
        skn_desc_dfo_attribs_sizew = 14,
        skn_desc_dfo_attribs_sizeh = 16,
        skn_desc_dfo_attribs_ext_attrib_flags = 18,
        skn_desc_dfo_attribs_reserved = 20,
        skn_desc_dfo_effect_queue_major = 8,
        skn_desc_dfo_effect_queue_minor = 12,
        skn_desc_dfo_effect_queue_ref_major = 16,
        skn_desc_dfo_effect_queue_ref_minor = 20,
        skn_desc_dfo_effect_queue_input_layer_index = 24,
        skn_desc_dfo_effect_queue_input_layer_mode = 25,
        skn_desc_dfo_effect_queue_output_layer_index = 26,
        skn_desc_dfo_effect_queue_output_layer_mode = 27,
        skn_desc_dfo_effect_queue_effect_count = 32,
        skn_desc_dfo_effect_queue_effects = 34,
        skn_desc_dfo_effect_uid = 8,
        skn_desc_dfo_effect_input_layerA_idx = 12,
        skn_desc_dfo_effect_input_layerA_mode = 13,
        skn_desc_dfo_effect_input_layerB_idx = 14,
        skn_desc_dfo_effect_input_layerB_mode = 15,
        skn_desc_dfo_effect_output_layer_idx = 16,
        skn_desc_dfo_effect_output_layer_mode = 17,
        skn_desc_dfo_effect_param_count = 18,
        skn_desc_dfo_effect_params = 20,
        skn_desc_dfo_param_len = 0,
        skn_desc_dfo_param_type = 3,
        skn_desc_dfo_release26_len = 0,
        skn_desc_dfo_release26_plat_major = 8,
        skn_desc_dfo_release26_plat_minor = 9,
        skn_desc_dfo_release26_chunks_count = 10,
        skn_desc_dfo_release26_content = 14,
        skn_desc_dfo_lang_gen_restr = 8,
        skn_desc_dfo_lang_lang_restr = 10,
        skn_desc_dfo_lang_lang_count = 12,
        skn_desc_dfo_lang_content = 16,
        skn_desc_dfo_release_generic_len = 0,
        skn_desc_dfo_release_generic_plat_major = 8,
        skn_desc_dfo_release_generic_plat_minor = 9,
        skn_desc_dfo_release_generic_chunks_count = 18,
        skn_desc_dfo_release_generic_content = 22,
        skn_desc_dfo_info_compiler_ver = 16,
        skn_desc_dfo_info_author_len = 24,
        skn_desc_dfo_info_author_str = 26
    };

    struct skn_name {
        std::uint16_t lang;
        std::u16string name;
    };

    struct skn_attrib_info {
        std::uint32_t attrib{ 0 };
        std::uint8_t align;
        std::int16_t image_coord_x;
        std::int16_t image_coord_y;
        std::uint16_t image_size_x;
        std::uint16_t image_size_y;
    };

    enum class skn_def_type {
        bitmap,
        img_tbl,
        color_tbl,
        bmp_anim
    };

    struct skn_def_base {
        std::uint64_t id_hash;
        skn_def_type type;
    };

    struct skn_bitmap_info : public skn_def_base {
        std::uint32_t filename_id;
        std::uint32_t bmp_idx;
        std::uint32_t mask_bitmap_idx;
        skn_attrib_info attrib;
    };

    struct skn_layout_info {
        std::int32_t layout_type;
        vec2 layout_size;
    };

    struct skn_image_table : public skn_def_base {
        std::vector<std::uint64_t> images;
        skn_attrib_info attrib;
    };

    struct skn_color_table : public skn_def_base {
        std::vector<std::pair<std::int16_t, common::rgba>> colors;
        skn_attrib_info attrib;
    };

    struct skn_anim_frame {
        std::uint64_t frame_bmp_hash;
        std::int16_t time;
        std::int16_t posx;
        std::int16_t posy;
    };

    struct skn_effect_parameter {
        std::uint8_t type;
        std::string data;
    };

    struct skn_effect {
        std::uint32_t uid;
        std::uint8_t input_layer_a_index;
        std::uint8_t input_layer_a_mode;
        std::uint8_t input_layer_b_index;
        std::uint8_t input_layer_b_mode;
        std::uint8_t output_layer_index;
        std::uint8_t output_layer_mode;
        std::vector<skn_effect_parameter> parameters;
    };

    struct skn_effect_queue : public skn_def_base {
        std::uint8_t input_layer_index;
        std::uint8_t input_layer_mode;
        std::uint8_t output_layer_index;
        std::uint8_t output_layer_mode;
        std::uint32_t ref_major;
        std::uint32_t ref_minor;
        std::vector<skn_effect> effects;
    };

    struct skn_bitmap_animation : public skn_def_base {
        std::int16_t interval;
        std::int16_t play_mode;
        std::vector<skn_anim_frame> frames;
        skn_attrib_info attrib;
    };

    struct skn_file_info {
        std::u16string author;
        std::u16string copyright;
        std::uint32_t version;
        std::uint16_t plat;
    };

    using plat_ver = std::pair<std::int8_t, std::int8_t>;

    struct skn_file {
        std::uint32_t master_chunk_size_;
        std::int32_t master_chunk_count_;
        std::uint32_t crr_filename_id_;
        skn_file_info info_;
        skn_name skin_name_;

        std::unordered_map<std::uint32_t, std::u16string> filenames_;
        std::map<std::uint64_t, skn_bitmap_info> bitmaps_;
        std::map<std::uint64_t, skn_image_table> img_tabs_;
        std::map<std::uint64_t, skn_color_table> color_tabs_;
        std::map<std::uint64_t, skn_bitmap_animation> bitmap_anims_;
        std::vector<skn_effect_queue> effect_queues_;

        common::ro_stream *stream_;
        plat_ver ver_;
        ::language importer_lang_;

        bool read_master_chunk();
        bool process_chunks(std::uint32_t base_offset, const std::int32_t count);
        void process_class_def_chunks(std::uint32_t base_offset, const std::int32_t count);
        void process_bitmap_def_chunk(std::uint32_t base_offset);
        void process_image_table_def_chunk(std::uint32_t base_offset);
        void process_color_table_def_chunk(std::uint32_t base_offset);
        void process_bitmap_anim_def_chunk(std::uint32_t base_offset);
        void process_attrib(std::uint32_t base_offset, skn_attrib_info &attrib);
        void process_effect_queue_chunk(std::uint32_t base_offset);
        void process_effects(std::uint32_t &base_offset, std::vector<skn_effect> &effects);
        void process_effect_parameters(std::uint32_t &base_offset, std::vector<skn_effect_parameter> &parameters);

        std::uint32_t handle_info_chunk(std::uint32_t base_offset, skn_file_info &info);
        std::uint32_t handle_name_chunk(std::uint32_t base_offset, skn_name &name);
        std::uint32_t handle_filename_chunk(std::uint32_t base_offset);
        std::uint32_t handle_class_chunk(std::uint32_t base_offset);
        std::uint32_t handle_release_26_restriction_chunk(std::uint32_t base_offset);
        std::uint32_t handle_release_generic_restriction_chunk(std::uint32_t base_offset);
        std::uint32_t handle_lang_restriction_chunk(std::uint32_t base_offset);
        std::string process_string(std::uint32_t base_offset, const std::uint16_t size);

        explicit skn_file(common::ro_stream *stream, plat_ver platform_version = { 2, 8 },
            ::language lang = ::language::any);
    };
}