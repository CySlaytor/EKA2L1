#include <services/applist/applist.h>
#include <services/fbs/fbs.h>

#include <common/benchmark.h>
#include <common/buffer.h>
#include <common/path.h>
#include <common/uid.h>

#include <loader/rsc.h>
#include <utils/cardinality.h>
#include <utils/consts.h>

namespace eka2l1 {
    static bool read_str16_aligned(common::ro_stream *stream, std::u16string &dat) {
        char len = 0;
        if (stream->read(&len, sizeof(len)) != sizeof(len)) {
            return false;
        }

        if (len == 0) {
            dat = u"";
            return true;
        } else if (len < 0) {
            return false;
        }

        if (stream->tell() % 2 != 0) {
            stream->seek(1, common::seek_where::cur);
        }

        dat.resize(len);
        if (stream->read(&dat[0], 2 * dat.length()) != 2 * dat.length()) {
            return false;
        }
        return true;
    }

    static bool read_non_localisable_info(common::ro_stream *stream, apa_app_registry &reg, const drive_number land_drive) {
        if (!read_str16_aligned(stream, reg.localised_info_rsc_path)) {
            return false;
        }

        if (stream->read(&reg.localised_info_rsc_id, 4) != 4) {
            return false;
        }

        std::int8_t temp = 0;
        if (stream->read(&temp, 1) != 1)
            return false;
        reg.caps.is_hidden = temp;

        if (stream->read(&temp, 1) != 1)
            return false;
        reg.caps.ability = static_cast<apa_capability::embeddability>(temp);

        if (stream->read(&temp, 1) != 1)
            return false;
        reg.caps.support_being_asked_to_create_new_file = temp;

        if (stream->read(&temp, 1) != 1)
            return false;
        reg.caps.launch_in_background = temp;

        std::u16string group_name;
        if (!read_str16_aligned(stream, group_name))
            return false;
        reg.caps.group_name = group_name;

        if (stream->read(&reg.default_screen_number, 1) != 1)
            return false;

        return true;
    }

    static bool read_mandatory_info(common::ro_stream *stream, apa_app_registry &reg, const drive_number land_drive, const bool app_path_oldarch) {
        stream->seek(8, common::seek_where::beg);
        std::u16string app_file;

        if (!read_str16_aligned(stream, app_file)) {
            return false;
        }

        if (stream->read(&reg.caps.flags, sizeof(reg.caps.flags)) != sizeof(reg.caps.flags)) {
            return false;
        }

        std::u16string binary_name = eka2l1::filename(app_file);
        if (binary_name.back() == u'\0') {
            binary_name.pop_back();
        }

        if (reg.caps.flags & apa_capability::non_native) {
            reg.mandatory_info.app_path = std::u16string(1, drive_to_char16(land_drive)) + eka2l1::relative_path(app_file);
        } else if (reg.caps.flags & apa_capability::built_as_dll) {
            reg.mandatory_info.app_path = std::u16string(1, drive_to_char16(land_drive)) + (app_path_oldarch ? u":\\system\\programs\\" : u":\\")
                + binary_name + u".dll";
        } else {
            reg.mandatory_info.app_path = std::u16string(1, drive_to_char16(land_drive)) + (app_path_oldarch ? u":\\system\\programs\\" : u":\\")
                + binary_name + u".exe";
        }
        return true;
    }

    bool read_registeration_info(common::ro_stream *stream, apa_app_registry &reg, const drive_number land_drive, const bool app_path_oldarch) {
        if (!read_mandatory_info(stream, reg, land_drive, app_path_oldarch))
            return false;
        if (!read_non_localisable_info(stream, reg, land_drive))
            return true;
        return true;
    }

    bool read_registeration_info_aif(common::ro_stream *stream, apa_app_registry &reg, const drive_number land_drive, language lang) {
        return read_registeration_info(stream, reg, land_drive, false);
    }

    bool read_localised_registration_info(common::ro_stream *stream, apa_app_registry &reg, const drive_number land_drive) {
        stream->seek(8, common::seek_where::beg);
        std::u16string cap;
        if (!read_str16_aligned(stream, cap))
            return false;

        reg.mandatory_info.short_caption = cap;
        stream->seek(8, common::seek_where::cur);
        if (!read_str16_aligned(stream, cap))
            return false;

        reg.mandatory_info.long_caption = cap;

        if (stream->read(&reg.icon_count, 2) != 2)
            return false;
        if (!read_str16_aligned(stream, cap))
            return false;

        reg.icon_file_path = cap;

        std::uint16_t view_count = 0;
        if (stream->read(&view_count, 2) != 2)
            return false;

        reg.view_datas.resize(view_count);
        for (std::uint16_t i = 0; i < view_count; i++) {
            stream->seek(8, common::seek_where::cur);
            if (stream->read(&reg.view_datas[i].uid_, 4) != 4)
                return false;
            if (stream->read(&reg.view_datas[i].screen_mode_, 4) != 4)
                return false;
            stream->seek(8, common::seek_where::cur);
            if (!read_str16_aligned(stream, reg.view_datas[i].caption_))
                return false;

            std::uint16_t icon_count_temp = 0;
            if (stream->read(&icon_count_temp, 2) != 2)
                return false;
            reg.view_datas[i].icon_count_ = icon_count_temp;
            if (!read_str16_aligned(stream, reg.view_datas[i].icon_path_))
                return false;
        }

        return true;
    }
}