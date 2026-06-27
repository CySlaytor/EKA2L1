#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <common/vecx.h>
#include <services/fbs/font.h>

namespace eka2l1::epoc::adapter {
    struct character_info {
        std::uint16_t x0;
        std::uint16_t y0;
        std::uint16_t x1;
        std::uint16_t y1;
        float xoff;
        float yoff;
        float xadv;
        float xoff2;
        float yoff2;
    };

    static constexpr std::uint32_t INVALID_FONT_TF_UID = 0xFFFFFFFF;

    class font_file_adapter_base {
    public:
        virtual ~font_file_adapter_base() {}

        virtual bool is_valid() = 0;
        virtual bool vectorizable() const = 0;
        virtual std::uint32_t line_gap(const std::size_t idx, const std::uint32_t metric_identifier) { return 0; }
        virtual bool get_face_attrib(const std::size_t idx, open_font_face_attrib &face_attrib) = 0;
        virtual std::uint8_t *get_glyph_bitmap(const std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier,
            int *rasterized_width, int *rasterized_height, std::uint32_t &total_size, epoc::glyph_bitmap_type *bmp_type,
            open_font_character_metric &character_metric)
            = 0;
        virtual void free_glyph_bitmap(std::uint8_t *data) = 0;
        virtual glyph_bitmap_type get_output_bitmap_type() const = 0;
        virtual bool does_glyph_exist(std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier) = 0;
        virtual std::size_t count() = 0;
        virtual std::optional<open_font_metrics> get_nearest_supported_metric(const std::size_t face_index, const std::uint16_t targeted_font_size,
            std::uint32_t *metric_identifier = nullptr, bool is_design_font_size = true)
            = 0;
    };

    enum class font_file_adapter_kind {
        none,
        freetype
    };

    using font_file_adapter_instance = std::unique_ptr<font_file_adapter_base>;
    font_file_adapter_instance make_font_file_adapter(const font_file_adapter_kind kind, std::vector<std::uint8_t> &dat);
}