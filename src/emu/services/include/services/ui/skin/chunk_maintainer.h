#pragma once

#include <cstdint>
#include <services/fbs/fbs.h>
#include <services/ui/skin/common.h>
#include <services/ui/skin/skn.h>
#include <string>
#include <vector>

namespace eka2l1::kernel {
    class chunk;
}

namespace eka2l1::epoc {
    struct skn_file;
    struct skn_bitmap_info;
    struct skn_color_table;
    struct skn_image_table;
    struct skn_effect_queue;
    struct skn_effect;
    struct skn_effect_parameter;

    enum class akn_skin_chunk_area_base_offset {
        item_def_area_base = 0,
        item_def_area_allocated_size = 1,
        item_def_area_current_size = 2,
        data_area_base = 3,
        data_area_allocated_size = 4,
        data_area_current_size = 5,
        filename_area_base = 6,
        filename_area_allocated_size = 7,
        filename_area_current_size = 8,
        gfx_area_base = 9,
        gfx_area_allocated_size = 10,
        gfx_area_current_size = 11,
        item_def_hash_base = 12,
        item_def_hash_allocated_size = 13,
        item_def_hash_current_size = 14,
        base_offset_end
    };

    enum akn_skin_chunk_maintainer_flag {
        akn_skin_chunk_maintainer_lookup_use_linked_list = 1 << 0
    };

    constexpr std::uint32_t AKN_SKIN_SERVER_MAX_FILENAME_LENGTH = 512;
    constexpr std::uint32_t AKN_SKIN_SERVER_MAX_FILENAME_BYTES = AKN_SKIN_SERVER_MAX_FILENAME_LENGTH * 2;

    class akn_skin_chunk_maintainer {
        struct akn_skin_chunk_area {
            akn_skin_chunk_area_base_offset base_;
            std::size_t gran_off_;
            std::int64_t gran_size_;
        };

        kernel::chunk *shared_chunk_;
        std::size_t current_granularity_off_;
        std::size_t max_size_gran_;
        std::size_t granularity_;
        std::vector<akn_skin_chunk_area> areas_;
        std::uint32_t level_;
        std::uint32_t flags_;

        const std::uint32_t current_filename_count();
        bool add_area(const akn_skin_chunk_area_base_offset offset_type, const std::int64_t allocated_size_gran);
        akn_skin_chunk_area *get_area_info(const akn_skin_chunk_area_base_offset area_type);

        bool import_bitmap(const skn_bitmap_info &info);
        bool import_color_table(const skn_color_table &table);
        bool import_image_table(const skn_image_table &table);
        bool import_effect_queue(const skn_effect_queue &queue);

    public:
        explicit akn_skin_chunk_maintainer(kernel::chunk *shared_chunk, const std::size_t granularity,
            const std::uint32_t flags = 0);

        const std::size_t get_area_size(const akn_skin_chunk_area_base_offset area_type, const bool paper_calc = false);
        const std::size_t get_area_current_size(const akn_skin_chunk_area_base_offset area_type);
        bool set_area_current_size(const akn_skin_chunk_area_base_offset area_type, const std::uint32_t new_size);

        std::int32_t update_data(const std::uint8_t *new_data, std::uint8_t *old_data, const std::size_t new_size, const std::size_t old_size);
        void *get_area_base(const akn_skin_chunk_area_base_offset area_type, std::uint64_t *offset_from_begin = nullptr);

        bool update_filename(const std::uint32_t filename_id, const std::u16string &filename, const std::u16string &filename_base);
        std::int32_t get_filename_offset_from_id(const std::uint32_t filename_id);
        std::int32_t get_item_definition_index(const epoc::pid &id);
        bool update_definition(const epoc::akns_item_def &def, const void *data, const std::size_t data_size, const std::size_t old_data_size);

        bool import(skn_file &skn, const std::u16string &filename_base);
        akns_item_def *get_item_definition(const epoc::pid &id);

        const std::uint32_t level() const { return level_; }
    };
}