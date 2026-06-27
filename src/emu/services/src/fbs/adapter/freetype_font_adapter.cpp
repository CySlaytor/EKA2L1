#include <common/cvt.h>
#include <common/log.h>
#include <freetype/tttables.h>
#include <memory>
#include <services/fbs/adapter/freetype_font_adapter.h>

namespace eka2l1::epoc::adapter {
    struct freetype_lib_raii {
        FT_Library lib_{};

        explicit freetype_lib_raii() {
            auto err = FT_Init_FreeType(&lib_);
            if (err) {
                LOG_ERROR(SERVICE_FBS, "Failed to initialize FreeType library, error: {}", FT_Error_String(err));
            }
        }

        ~freetype_lib_raii() {
            FT_Done_FreeType(lib_);
        }
    };

    std::unique_ptr<freetype_lib_raii> ft_lib_raii_;

    static int derive_design_height_from_max_height(const FT_Face &aFace, int aMaxHeightInPixel) {
        const int boundingBoxHeightInFontUnit = aFace->bbox.yMax - aFace->bbox.yMin;
        int designHeightInPixels = ((aMaxHeightInPixel * aFace->units_per_EM) / boundingBoxHeightInFontUnit);

        const int maxHeightInFontUnit = aMaxHeightInPixel << 6;
        FT_Set_Pixel_Sizes(aFace, designHeightInPixels, designHeightInPixels);
        int currentMaxHeightInFontUnit = FT_MulFix(
            boundingBoxHeightInFontUnit, aFace->size->metrics.y_scale);
        while (currentMaxHeightInFontUnit < maxHeightInFontUnit) {
            designHeightInPixels++;
            FT_Set_Pixel_Sizes(aFace, designHeightInPixels, designHeightInPixels);
            currentMaxHeightInFontUnit = FT_MulFix(
                boundingBoxHeightInFontUnit, aFace->size->metrics.y_scale);
        }
        while (currentMaxHeightInFontUnit > maxHeightInFontUnit) {
            designHeightInPixels--;
            FT_Set_Pixel_Sizes(aFace, designHeightInPixels, designHeightInPixels);
            currentMaxHeightInFontUnit = FT_MulFix(
                boundingBoxHeightInFontUnit, aFace->size->metrics.y_scale);
        }
        return designHeightInPixels;
    }

    inline short ft_convention_to_int_pixel(FT_Pos val) {
        return static_cast<short>((val + 32) >> 6);
    }

    FT_Library get_ft_lib() {
        return ft_lib_raii_->lib_;
    }

    freetype_font_adapter::freetype_font_adapter(std::vector<std::uint8_t> &data)
        : data_(data)
        , is_valid_(false) {
        if (!ft_lib_raii_) {
            ft_lib_raii_ = std::make_unique<freetype_lib_raii>();
        }

        FT_Face query_face;
        auto err = FT_New_Memory_Face(get_ft_lib(), data_.data(), static_cast<FT_Long>(data_.size()), -1, &query_face);
        if (err) {
            LOG_ERROR(SERVICE_FBS, "FreeType failed to query face count from memory, error: {}", FT_Error_String(err));
            return;
        }

        for (auto i = 0; i < query_face->num_faces; i++) {
            FT_Face face;
            err = FT_New_Memory_Face(get_ft_lib(), data_.data(), static_cast<FT_Long>(data_.size()), i, &face);
            if (err) {
                LOG_ERROR(SERVICE_FBS, "FreeType failed to read face from memory, error: {}", FT_Error_String(err));
                continue;
            }
            faces_.push_back(face);
        }

        if (!faces_.empty()) {
            is_valid_ = true;
            current_font_sizes_.resize(faces_.size());
            std::fill(current_font_sizes_.begin(), current_font_sizes_.end(), 0);
        }
    }

    bool freetype_font_adapter::set_font_size(const std::size_t index, const std::uint32_t size) {
        if (index >= faces_.size()) {
            return false;
        }
        if (current_font_sizes_[index] != 0 && current_font_sizes_[index] == size) {
            return true;
        }
        auto face = faces_[index];
        auto err = FT_Set_Pixel_Sizes(face, 0, size);
        if (err) {
            LOG_ERROR(SERVICE_FBS, "Failed to set character size for face, error: {}", FT_Error_String(err));
            return false;
        }
        current_font_sizes_[index] = size;
        return true;
    }

    freetype_font_adapter::~freetype_font_adapter() {
        for (auto face : faces_) {
            FT_Done_Face(face);
        }
    }

    std::uint32_t freetype_font_adapter::line_gap(const std::size_t idx, const std::uint32_t metric_identifier) {
        if (idx >= faces_.size())
            return 0;
        if (!set_font_size(idx, metric_identifier))
            return 0;
        return faces_[idx]->size->metrics.height - faces_[idx]->size->metrics.ascender + faces_[idx]->size->metrics.descender;
    }

    bool freetype_font_adapter::get_face_attrib(const std::size_t idx, open_font_face_attrib &face_attrib) {
        if (idx >= faces_.size())
            return false;

        auto face = faces_[idx];
        auto fam_name = common::utf8_to_ucs2(face->family_name);
        auto name = fam_name + u" " + common::utf8_to_ucs2(face->style_name);

        face_attrib.fam_name.assign(nullptr, fam_name);
        face_attrib.name.assign(nullptr, name);
        face_attrib.local_full_fam_name.assign(nullptr, fam_name);
        face_attrib.local_full_name.assign(nullptr, name);
        face_attrib.style = 0;

        if (face->style_flags & FT_STYLE_FLAG_BOLD)
            face_attrib.style |= open_font_face_attrib::bold;
        if (face->style_flags & FT_STYLE_FLAG_ITALIC)
            face_attrib.style |= open_font_face_attrib::italic;
        if (face->face_flags & FT_FACE_FLAG_FIXED_WIDTH)
            face_attrib.style |= open_font_face_attrib::mono_width;

        auto header = reinterpret_cast<TT_Header *>(FT_Get_Sfnt_Table(face, FT_SFNT_HEAD));
        if (header)
            face_attrib.min_size_in_pixels = header->Lowest_Rec_PPEM;

        auto os2 = reinterpret_cast<TT_OS2 *>(FT_Get_Sfnt_Table(face, FT_SFNT_OS2));
        if (os2) {
            face_attrib.coverage[0] = os2->ulUnicodeRange1;
            face_attrib.coverage[1] = os2->ulUnicodeRange2;
            face_attrib.coverage[2] = os2->ulUnicodeRange3;
            face_attrib.coverage[3] = os2->ulUnicodeRange4;
            if (((os2->panose[1] >= 2) && (os2->panose[1] <= 10)) || (os2->panose[1] >= 14)) {
                face_attrib.style |= open_font_face_attrib::serif;
            }
        }
        return true;
    }

    std::uint8_t *freetype_font_adapter::get_glyph_bitmap(const std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier,
        int *rasterized_width, int *rasterized_height, std::uint32_t &total_size, epoc::glyph_bitmap_type *bmp_type,
        open_font_character_metric &character_metric) {
        if (idx >= faces_.size())
            return nullptr;

        auto face = faces_[idx];
        auto glyph_index = code;
        if (glyph_index & 0x80000000)
            glyph_index &= ~0x80000000;
        else
            glyph_index = FT_Get_Char_Index(face, code);

        if (!set_font_size(idx, metric_identifier))
            return nullptr;

        auto err = FT_Load_Glyph(face, glyph_index, FT_LOAD_RENDER);
        if (err) {
            LOG_ERROR(SERVICE_FBS, "Failed to load glyph for face to get glyph bitmap, error: {}", FT_Error_String(err));
            return nullptr;
        }

        auto glyph = face->glyph;
        auto bitmap = glyph->bitmap;

        if (rasterized_width)
            *rasterized_width = static_cast<int>(bitmap.width);
        if (rasterized_height)
            *rasterized_height = static_cast<int>(bitmap.rows);
        total_size = bitmap.width * bitmap.rows;
        if (bmp_type)
            *bmp_type = glyph_bitmap_type::antialised_glyph_bitmap;

        character_metric.width = ft_convention_to_int_pixel(glyph->metrics.width);
        character_metric.height = ft_convention_to_int_pixel(glyph->metrics.height);
        character_metric.horizontal_bearing_x = ft_convention_to_int_pixel(glyph->metrics.horiBearingX);
        character_metric.horizontal_bearing_y = ft_convention_to_int_pixel(glyph->metrics.horiBearingY);
        character_metric.horizontal_advance = ft_convention_to_int_pixel(glyph->metrics.horiAdvance);
        character_metric.vertical_bearing_x = ft_convention_to_int_pixel(glyph->metrics.vertBearingX);
        character_metric.vertical_bearing_y = ft_convention_to_int_pixel(glyph->metrics.vertBearingY);
        character_metric.vertical_advance = ft_convention_to_int_pixel(glyph->metrics.vertAdvance);
        character_metric.bitmap_type = glyph_bitmap_type::antialised_glyph_bitmap;

        return bitmap.buffer;
    }

    void freetype_font_adapter::free_glyph_bitmap(std::uint8_t *data) {}

    bool freetype_font_adapter::does_glyph_exist(std::size_t idx, std::uint32_t code, const std::uint32_t metric_identifier) {
        if (idx >= faces_.size())
            return false;
        auto face = faces_[idx];
        if (code & 0x80000000) {
            return ((code & ~0x80000000) < face->num_glyphs);
        }
        return FT_Get_Char_Index(face, code) != 0;
    }

    std::size_t freetype_font_adapter::count() {
        return faces_.size();
    }

    std::optional<open_font_metrics> freetype_font_adapter::get_nearest_supported_metric(const std::size_t face_index, const std::uint16_t targeted_font_size, std::uint32_t *metric_identifier,
        bool is_design_font_size) {
        if (face_index >= faces_.size())
            return std::nullopt;

        auto face = faces_[face_index];
        auto adjusted_font_size = is_design_font_size ? (static_cast<int>(static_cast<float>(targeted_font_size) * (9.0f / 10.0f))) : derive_design_height_from_max_height(face, targeted_font_size);
        auto fake_design_height = is_design_font_size ? targeted_font_size : adjusted_font_size;

        if (!set_font_size(face_index, adjusted_font_size))
            return std::nullopt;

        open_font_metrics metrics{};
        metrics.ascent = ft_convention_to_int_pixel(face->size->metrics.ascender);
        metrics.descent = ft_convention_to_int_pixel(face->size->metrics.descender);
        metrics.max_height = ft_convention_to_int_pixel(face->size->metrics.height);
        metrics.max_width = ft_convention_to_int_pixel(face->size->metrics.max_advance);
        metrics.max_depth = 0;
        metrics.baseline_correction = 0;
        metrics.design_height = static_cast<std::int16_t>(fake_design_height);

        if (metric_identifier) {
            *metric_identifier = adjusted_font_size;
        }

        return metrics;
    }
}