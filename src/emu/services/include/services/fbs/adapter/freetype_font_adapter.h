#pragma once

#include <map>
#include <memory>
#include <vector>

#include <common/container.h>
#include <freetype/freetype.h>
#include <services/fbs/adapter/font_adapter.h>

namespace eka2l1::epoc::adapter {
    class freetype_font_adapter : public font_file_adapter_base {
    private:
        std::vector<std::uint8_t> data_;
        std::vector<FT_Face> faces_;
        bool is_valid_;
        std::vector<std::uint32_t> current_font_sizes_;

    protected:
        bool set_font_size(const std::size_t face_index, const std::uint32_t size);

    public:
        explicit freetype_font_adapter(std::vector<std::uint8_t> &data_);
        ~freetype_font_adapter() override;

        bool is_valid() override { return is_valid_; }
        bool vectorizable() const override { return true; }
        std::uint32_t line_gap(const std::size_t idx, const std::uint32_t metric_identifier) override;
        bool get_face_attrib(const std::size_t idx, open_font_face_attrib &face_attrib) override;
        std::uint8_t *get_glyph_bitmap(const std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier,
            int *rasterized_width, int *rasterized_height, std::uint32_t &total_size, epoc::glyph_bitmap_type *bmp_type,
            open_font_character_metric &character_metric) override;
        void free_glyph_bitmap(std::uint8_t *data) override;
        glyph_bitmap_type get_output_bitmap_type() const override { return antialised_glyph_bitmap; }
        bool does_glyph_exist(std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier) override;
        std::size_t count() override;
        std::optional<open_font_metrics> get_nearest_supported_metric(const std::size_t face_index, const std::uint16_t targeted_font_size,
            std::uint32_t *metric_identifier = nullptr, bool is_design_font_size = true) override;
    };
}