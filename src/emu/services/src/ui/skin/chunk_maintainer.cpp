#include <common/path.h>
#include <common/time.h>
#include <common/vecx.h>
#include <kernel/chunk.h>
#include <services/ui/skin/chunk_maintainer.h>
#include <services/ui/skin/skn.h>

namespace eka2l1::epoc {
    constexpr std::int64_t AKNS_CHUNK_ITEM_DEF_HASH_BASE_SIZE_GRAN = 2;
    constexpr std::int64_t AKNS_CHUNK_ITEM_DEF_AREA_BASE_SIZE_GRAN = 25;
    constexpr std::int64_t AKNS_CHUNK_DATA_AREA_BASE_SIZE_GRAN = 55;
    constexpr std::int64_t AKNS_CHUNK_FILENAME_AREA_BASE_SIZE_GRAN = 6;
    constexpr std::int64_t AKNS_CHUNK_SCALEABLE_GFX_AREA_BASE_SIZE_GRAN = 6;

    struct akns_srv_bitmap_def {
        akns_mtptr filename_;
        std::int32_t index_;
        std::int32_t image_attrib_;
        std::int32_t image_alignment_;
        std::int32_t image_x_coord_;
        std::int32_t image_y_coord_;
        std::int32_t image_width_;
        std::int32_t image_height_;
    };

    struct akns_srv_masked_bitmap_def {
        akns_mtptr filename_;
        std::int32_t index_;
        std::int32_t masked_index_;
        std::int32_t image_attrib_;
        std::int32_t image_alignment_;
        std::int32_t image_x_coord_;
        std::int32_t image_y_coord_;
        std::int32_t image_width_;
        std::int32_t image_height_;
    };

    struct akns_color_table_entry {
        std::int32_t index_;
        std::uint32_t rgb_;
    };

    struct akns_srv_color_table_def {
        std::int32_t count_;
        akns_mtptr entries_;
        std::int32_t image_attrib_;
        std::int32_t image_alignment_;
        std::int32_t image_x_coord_;
        std::int32_t image_y_coord_;
        std::int32_t image_width_;
        std::int32_t image_height_;
    };

    struct akns_srv_effect_queue_def {
        std::uint32_t size_;
        std::uint32_t input_layer_index_;
        std::uint32_t input_layer_mode_;
        std::uint32_t output_layer_index_;
        std::uint32_t output_layer_mode_;
        std::uint32_t count_;
        std::uint32_t ref_major_;
        std::uint32_t ref_minor_;
    };

    struct akns_srv_effect_def {
        std::uint32_t uid_;
        std::uint32_t input_layer_a_index_;
        std::uint32_t input_layer_a_mode_;
        std::uint32_t input_layer_b_index_;
        std::uint32_t input_layer_b_mode_;
        std::uint32_t output_layer_index_;
        std::uint32_t output_layer_mode_;
        std::uint32_t effect_parameter_count_;
        std::uint32_t effect_size_;
    };

    struct akns_srv_effect_parameter_def {
        std::uint32_t param_length_;
        std::uint32_t param_type_;
    };

    using akns_srv_image_table_def = akns_srv_color_table_def;

    static pid make_pid_from_id_hash(const std::uint64_t hash) {
        return { static_cast<std::int32_t>(hash), static_cast<std::int32_t>(hash >> 32) };
    }

    akn_skin_chunk_maintainer::akn_skin_chunk_maintainer(kernel::chunk *shared_chunk, const std::size_t granularity,
        const std::uint32_t flags)
        : shared_chunk_(shared_chunk)
        , current_granularity_off_(0)
        , max_size_gran_(0)
        , granularity_(granularity)
        , level_(0)
        , flags_(flags) {
        max_size_gran_ = shared_chunk_->max_size() / granularity;

        add_area(akn_skin_chunk_area_base_offset::item_def_hash_base, AKNS_CHUNK_ITEM_DEF_HASH_BASE_SIZE_GRAN);
        add_area(akn_skin_chunk_area_base_offset::item_def_area_base, AKNS_CHUNK_ITEM_DEF_AREA_BASE_SIZE_GRAN);
        add_area(akn_skin_chunk_area_base_offset::data_area_base, AKNS_CHUNK_DATA_AREA_BASE_SIZE_GRAN);
        add_area(akn_skin_chunk_area_base_offset::gfx_area_base, AKNS_CHUNK_SCALEABLE_GFX_AREA_BASE_SIZE_GRAN);
        add_area(akn_skin_chunk_area_base_offset::filename_area_base, AKNS_CHUNK_FILENAME_AREA_BASE_SIZE_GRAN);

        std::uint8_t *area = reinterpret_cast<std::uint8_t *>(get_area_base(akn_skin_chunk_area_base_offset::item_def_hash_base));
        std::fill(area, area + get_area_size(akn_skin_chunk_area_base_offset::item_def_hash_base), 0xFF);
    }

    const std::uint32_t akn_skin_chunk_maintainer::current_filename_count() {
        const std::size_t area_crr_size = get_area_current_size(akn_skin_chunk_area_base_offset::filename_area_base);
        if (area_crr_size == static_cast<std::size_t>(-1)) {
            return static_cast<std::uint32_t>(-1);
        }
        return static_cast<std::uint32_t>(area_crr_size / AKN_SKIN_SERVER_MAX_FILENAME_BYTES);
    }

    static std::uint32_t *search_filename_in_area(std::uint32_t *area, const std::uint32_t filename_id, const std::uint32_t count) {
        if (area == nullptr)
            return nullptr;
        for (std::uint32_t i = 0; i < count; i++) {
            if (area[0] == filename_id)
                return area;
            area += AKN_SKIN_SERVER_MAX_FILENAME_BYTES / 4;
        }
        return nullptr;
    }

    std::int32_t akn_skin_chunk_maintainer::get_filename_offset_from_id(const std::uint32_t filename_id) {
        std::uint32_t *areabase = reinterpret_cast<std::uint32_t *>(get_area_base(akn_skin_chunk_area_base_offset::filename_area_base));
        if (areabase == nullptr)
            return false;

        const std::uint32_t crr_fn_count = current_filename_count();
        std::uint32_t *area_ptr = search_filename_in_area(areabase, filename_id, crr_fn_count);

        if (!area_ptr)
            return -1;
        return static_cast<std::int32_t>((area_ptr - areabase) * sizeof(std::uint32_t)) + 4;
    }

    bool akn_skin_chunk_maintainer::update_filename(const std::uint32_t filename_id, const std::u16string &filename,
        const std::u16string &filename_base) {
        std::uint32_t *areabase = reinterpret_cast<std::uint32_t *>(get_area_base(akn_skin_chunk_area_base_offset::filename_area_base));
        if (areabase == nullptr)
            return false;

        const std::uint32_t crr_fn_count = current_filename_count();
        std::uint32_t *areaptr = search_filename_in_area(areabase, filename_id, crr_fn_count);

        if (!areaptr) {
            areaptr = areabase + crr_fn_count * AKN_SKIN_SERVER_MAX_FILENAME_BYTES / 4;
            set_area_current_size(akn_skin_chunk_area_base_offset::filename_area_base,
                static_cast<std::uint32_t>(get_area_current_size(akn_skin_chunk_area_base_offset::filename_area_base)
                    + AKN_SKIN_SERVER_MAX_FILENAME_BYTES));
        }

        areaptr[0] = filename_id;
        areaptr += 1;

        std::copy(filename_base.data(), filename_base.data() + filename_base.length(), reinterpret_cast<char16_t *>(areaptr));
        std::copy(filename.data(), filename.data() + filename.length(), reinterpret_cast<char16_t *>(reinterpret_cast<std::uint8_t *>(areaptr) + filename_base.length() * 2));

        std::fill(reinterpret_cast<std::uint8_t *>(areaptr) + filename_base.length() * 2 + filename.length() * 2,
            reinterpret_cast<std::uint8_t *>(areaptr) + AKN_SKIN_SERVER_MAX_FILENAME_BYTES, 0);

        return true;
    }

    static constexpr std::int32_t MAX_HASH_AVAIL = 128;
    static std::uint32_t calculate_item_hash(const epoc::pid &id) {
        return (id.first + id.second) % MAX_HASH_AVAIL;
    }

    std::int32_t akn_skin_chunk_maintainer::get_item_definition_index(const epoc::pid &id) {
        if (flags_ & akn_skin_chunk_maintainer_lookup_use_linked_list) {
            std::int32_t *hash = reinterpret_cast<std::int32_t *>(get_area_base(epoc::akn_skin_chunk_area_base_offset::item_def_hash_base));
            epoc::akns_item_def *defs = reinterpret_cast<decltype(defs)>(get_area_base(epoc::akn_skin_chunk_area_base_offset::item_def_area_base));
            std::uint32_t hash_index = calculate_item_hash(id);
            std::int32_t head = hash[hash_index];

            if (head < 0)
                return -1;
            while (head >= 0 && (defs[head].id_ != id)) {
                head = defs[head].next_hash_;
            }
            return head;
        }

        epoc::akns_item_def_v1 *items = reinterpret_cast<epoc::akns_item_def_v1 *>(
            get_area_base(epoc::akn_skin_chunk_area_base_offset::item_def_area_base));
        const std::size_t total_items = get_area_current_size(epoc::akn_skin_chunk_area_base_offset::item_def_area_base)
            / sizeof(epoc::akns_item_def_v1);

        epoc::akns_item_def_v1 *target_item = std::find_if(items, items + total_items, [=](const akns_item_def_v1 &this_item) {
            return this_item.id_ == id;
        });

        if (target_item == items + total_items)
            return -1;
        return static_cast<std::int32_t>(std::distance(items, target_item));
    }

    std::int32_t akn_skin_chunk_maintainer::update_data(const std::uint8_t *new_data, std::uint8_t *old_data, const std::size_t new_size, const std::size_t old_size) {
        std::int32_t offset = 0;
        if (old_data == nullptr || (old_size < new_size)) {
            offset = static_cast<std::int32_t>(get_area_current_size(epoc::akn_skin_chunk_area_base_offset::data_area_base));
            std::uint8_t *data_head = reinterpret_cast<std::uint8_t *>(get_area_base(epoc::akn_skin_chunk_area_base_offset::data_area_base)) + offset;
            set_area_current_size(epoc::akn_skin_chunk_area_base_offset::data_area_base, static_cast<std::uint32_t>(offset + new_size));

            if (new_data)
                std::copy(new_data, new_data + new_size, data_head);
            return offset;
        }

        if (new_data)
            std::copy(new_data, new_data + new_size, old_data);
        return static_cast<std::int32_t>(old_data - reinterpret_cast<std::uint8_t *>(get_area_base(epoc::akn_skin_chunk_area_base_offset::data_area_base)));
    }

    bool akn_skin_chunk_maintainer::update_definition(const epoc::akns_item_def &def, const void *data, const std::size_t data_size, const std::size_t old_data_size) {
        std::int32_t index = get_item_definition_index(def.id_);
        std::size_t old_data_size_to_update = 0;
        void *old_data = nullptr;
        akns_item_def *current_def = nullptr;
        std::uint32_t definition_size = sizeof(akns_item_def_v1);

        if (flags_ & akn_skin_chunk_maintainer_lookup_use_linked_list)
            definition_size = sizeof(akns_item_def_v2);

        if (index < 0) {
            const std::size_t def_size = get_area_current_size(epoc::akn_skin_chunk_area_base_offset::item_def_area_base);
            std::uint8_t *current_head = reinterpret_cast<std::uint8_t *>(get_area_base(epoc::akn_skin_chunk_area_base_offset::item_def_area_base)) + def_size;
            set_area_current_size(epoc::akn_skin_chunk_area_base_offset::item_def_area_base, static_cast<std::uint32_t>(def_size + definition_size));

            std::memcpy(current_head, &def, definition_size);
            if (flags_ & akn_skin_chunk_maintainer_lookup_use_linked_list) {
                std::int32_t head = calculate_item_hash(reinterpret_cast<akns_item_def *>(current_head)->id_);
                std::int32_t *hash = reinterpret_cast<std::int32_t *>(get_area_base(epoc::akn_skin_chunk_area_base_offset::item_def_hash_base));
                reinterpret_cast<akns_item_def *>(current_head)->next_hash_ = hash[head];
                hash[head] = static_cast<std::int32_t>(def_size / sizeof(akns_item_def_v2));
            }
            current_def = reinterpret_cast<akns_item_def *>(current_head);
        } else {
            current_def = reinterpret_cast<akns_item_def *>(get_area_base(epoc::akn_skin_chunk_area_base_offset::item_def_area_base)) + index;
            if (current_def->type_ == def.type_ && current_def->data_.type_ == epoc::akns_mtptr_type::akns_mtptr_type_relative_ram) {
                std::uint32_t *data_size_ptr = current_def->data_.get_relative<std::uint32_t>(get_area_base(epoc::akn_skin_chunk_area_base_offset::data_area_base));
                if (old_data_size == 0xFFFFFFFF)
                    old_data_size_to_update = *data_size_ptr;
                else
                    old_data_size_to_update = old_data_size;
                old_data = data_size_ptr;
            }

            std::int32_t head = 0;
            if (flags_ & akn_skin_chunk_maintainer_lookup_use_linked_list)
                head = current_def->next_hash_;
            std::memcpy(current_def, &def, definition_size);
            if (flags_ & akn_skin_chunk_maintainer_lookup_use_linked_list)
                current_def->next_hash_ = head;
        }

        current_def->data_.type_ = akns_mtptr_type_relative_ram;
        current_def->data_.address_or_offset_ = update_data(reinterpret_cast<const std::uint8_t *>(data), reinterpret_cast<std::uint8_t *>(old_data), data_size, old_data_size_to_update);
        return true;
    }

    bool akn_skin_chunk_maintainer::add_area(const akn_skin_chunk_area_base_offset offset_type, const std::int64_t allocated_size_gran) {
        if (static_cast<int>(offset_type) % 3 != 0)
            return false;

        akn_skin_chunk_area area;
        area.base_ = offset_type;
        area.gran_off_ = current_granularity_off_;
        area.gran_size_ = allocated_size_gran;

        if (get_area_info(offset_type))
            return false;

        std::size_t gran_to_increase_off = static_cast<std::size_t>(std::max<std::int64_t>(1ULL, allocated_size_gran));
        if (current_granularity_off_ + gran_to_increase_off > max_size_gran_)
            return false;

        std::uint32_t *base = reinterpret_cast<std::uint32_t *>(shared_chunk_->host_base());

        if (!areas_.empty() && base[static_cast<int>(areas_[0].base_) + 1] <= 4 * 3)
            return false;

        base[static_cast<int>(offset_type)] = static_cast<std::uint32_t>(current_granularity_off_ * granularity_);

        if (allocated_size_gran < 0)
            base[static_cast<int>(offset_type) + 1] = static_cast<std::uint32_t>(granularity_ / allocated_size_gran);
        else
            base[static_cast<int>(offset_type) + 1] = static_cast<std::uint32_t>(allocated_size_gran * granularity_);

        base[static_cast<int>(offset_type) + 2] = 0;

        areas_.emplace_back(area);

        const std::uint32_t header_size = static_cast<std::uint32_t>(akn_skin_chunk_area_base_offset::base_offset_end) * 4;
        base[static_cast<int>(areas_[0].base_)] = header_size;
        base[static_cast<int>(areas_[0].base_) + 1] = static_cast<std::uint32_t>(get_area_size(areas_[0].base_, true) - header_size);

        current_granularity_off_ += gran_to_increase_off;
        return true;
    }

    akn_skin_chunk_maintainer::akn_skin_chunk_area *akn_skin_chunk_maintainer::get_area_info(const akn_skin_chunk_area_base_offset area_type) {
        if (static_cast<int>(area_type) % 3 != 0)
            return nullptr;
        auto search_result = std::find_if(areas_.begin(), areas_.end(), [area_type](const akn_skin_chunk_area &lhs) {
            return lhs.base_ == area_type;
        });
        if (search_result == areas_.end())
            return nullptr;
        return &(*search_result);
    }

    const std::size_t akn_skin_chunk_maintainer::get_area_size(const akn_skin_chunk_area_base_offset area_type, const bool paper_calc) {
        akn_skin_chunk_area *area = get_area_info(area_type);
        if (!area)
            return static_cast<std::size_t>(-1);
        if (paper_calc) {
            if (area->gran_size_ < 0)
                return granularity_ / (-1 * area->gran_size_);
            return granularity_ * area->gran_size_;
        }
        std::uint32_t *base = reinterpret_cast<std::uint32_t *>(shared_chunk_->host_base());
        return static_cast<std::size_t>(base[static_cast<int>(area_type) + 1]);
    }

    void *akn_skin_chunk_maintainer::get_area_base(const akn_skin_chunk_area_base_offset area_type, std::uint64_t *offset_from_begin) {
        akn_skin_chunk_area *area = get_area_info(area_type);
        if (!area)
            return nullptr;
        std::uint32_t *base = reinterpret_cast<std::uint32_t *>(shared_chunk_->host_base());
        if (offset_from_begin)
            *offset_from_begin = base[static_cast<int>(area_type)];
        return reinterpret_cast<std::uint8_t *>(shared_chunk_->host_base()) + (base[static_cast<int>(area_type)]);
    }

    const std::size_t akn_skin_chunk_maintainer::get_area_current_size(const akn_skin_chunk_area_base_offset area_type) {
        akn_skin_chunk_area *area = get_area_info(area_type);
        if (!area)
            return static_cast<std::size_t>(-1);
        std::uint32_t *base = reinterpret_cast<std::uint32_t *>(shared_chunk_->host_base());
        return static_cast<std::size_t>(base[static_cast<int>(area_type) + 2]);
    }

    bool akn_skin_chunk_maintainer::set_area_current_size(const akn_skin_chunk_area_base_offset area_type, const std::uint32_t new_size) {
        akn_skin_chunk_area *area = get_area_info(area_type);
        if (!area)
            return false;
        std::uint32_t *base = reinterpret_cast<std::uint32_t *>(shared_chunk_->host_base());
        base[static_cast<int>(area_type) + 2] = new_size;
        return true;
    }

    akns_item_def *akn_skin_chunk_maintainer::get_item_definition(const epoc::pid &id) {
        const std::int32_t index = get_item_definition_index(id);
        if (index < 0)
            return nullptr;
        akns_item_def *defs = reinterpret_cast<akns_item_def *>(get_area_base(akn_skin_chunk_area_base_offset::item_def_area_base));
        if (!defs)
            return nullptr;
        return defs + index;
    }

    bool akn_skin_chunk_maintainer::import_color_table(const skn_color_table &table) {
        akns_item_def item;
        item.type_ = akns_item_type_color_table;
        item.id_ = make_pid_from_id_hash(table.id_hash);

        akns_srv_color_table_def color_table;
        color_table.count_ = static_cast<std::int32_t>(table.colors.size());
        color_table.entries_.type_ = akns_mtptr_type::akns_mtptr_type_relative_ram;

        akns_item_def *last_color_table_item = get_item_definition(item.id_);
        std::uint8_t *data_area = reinterpret_cast<std::uint8_t *>(get_area_base(akn_skin_chunk_area_base_offset::data_area_base));
        std::uint8_t *old_color_entries_data = nullptr;
        std::size_t old_size = 0;

        if (last_color_table_item) {
            akns_srv_color_table_def *last_color_table = last_color_table_item->data_.get_relative<akns_srv_color_table_def>(data_area);
            if (last_color_table) {
                old_color_entries_data = last_color_table->entries_.get_relative<std::uint8_t>(data_area);
                if (last_color_table->count_ > color_table.count_)
                    color_table.count_ = last_color_table->count_;
                old_size = last_color_table->count_ * sizeof(akns_color_table_entry);
            }
        }

        color_table.entries_.type_ = akns_mtptr_type_relative_ram;
        color_table.entries_.address_or_offset_ = update_data(nullptr, old_color_entries_data, color_table.count_ * sizeof(akns_color_table_entry), old_size);

        akns_color_table_entry *entries_to_fill = color_table.entries_.get_relative<akns_color_table_entry>(data_area);
        std::size_t index_ite = 0;
        for (const auto &entry : table.colors) {
            entries_to_fill[index_ite].index_ = entry.first;
            entries_to_fill[index_ite].rgb_ = entry.second;
            index_ite++;
        }

        color_table.image_alignment_ = table.attrib.align;
        color_table.image_attrib_ = table.attrib.attrib;
        color_table.image_height_ = table.attrib.image_size_y;
        color_table.image_width_ = table.attrib.image_size_x;
        color_table.image_x_coord_ = table.attrib.image_coord_x;
        color_table.image_y_coord_ = table.attrib.image_coord_y;

        return update_definition(item, &color_table, sizeof(akns_srv_color_table_def), sizeof(akns_srv_color_table_def));
    }

    static bool import_effect(akn_skin_chunk_maintainer &maintainer, const skn_effect &effect, std::vector<std::uint8_t> &effects) {
        effects.resize(sizeof(akns_srv_effect_def));
        akns_srv_effect_def *srv_effect = reinterpret_cast<decltype(srv_effect)>(&effects[0]);
        srv_effect->uid_ = effect.uid;
        srv_effect->input_layer_a_index_ = effect.input_layer_a_index;
        srv_effect->input_layer_b_index_ = effect.input_layer_b_index;
        srv_effect->input_layer_a_mode_ = effect.input_layer_a_mode;
        srv_effect->input_layer_b_mode_ = effect.input_layer_b_mode;
        srv_effect->output_layer_index_ = effect.output_layer_index;
        srv_effect->output_layer_mode_ = effect.output_layer_mode;
        srv_effect->effect_parameter_count_ = static_cast<std::uint32_t>(effect.parameters.size());
        srv_effect->effect_size_ = sizeof(akns_srv_effect_def);

        for (std::size_t i = 0; i < effect.parameters.size(); i++) {
            akns_srv_effect_parameter_def parameter;
            parameter.param_type_ = effect.parameters[i].type;
            parameter.param_length_ = static_cast<std::uint32_t>(effect.parameters[i].data.length()) + sizeof(akns_srv_effect_parameter_def);
            std::uint32_t to_pad = 0;

            if (parameter.param_length_ % 4 != 0) {
                const std::uint32_t aligned = common::align(parameter.param_length_, 4);
                to_pad = aligned - parameter.param_length_;
                parameter.param_length_ = aligned;
            }

            effects.insert(effects.end(), reinterpret_cast<std::uint8_t *>(&parameter), reinterpret_cast<std::uint8_t *>(&parameter) + sizeof(akns_srv_effect_parameter_def));

            if (parameter.param_type_ == 2) {
                const std::uint32_t filename_id = maintainer.level() + *reinterpret_cast<const std::uint32_t *>(&effect.parameters[i].data[effect.parameters[i].data.length() - 4]);
                const std::uint32_t offset = maintainer.get_filename_offset_from_id(filename_id);
                effects.insert(effects.end(), reinterpret_cast<const std::uint8_t *>(&effect.parameters[i].data[0]), reinterpret_cast<const std::uint8_t *>(&effect.parameters[i].data[effect.parameters[i].data.length() - 4]));
                effects.insert(effects.end(), reinterpret_cast<const std::uint8_t *>(&offset), reinterpret_cast<const std::uint8_t *>(&offset + 1));
            } else {
                effects.insert(effects.end(), reinterpret_cast<const std::uint8_t *>(&effect.parameters[i].data[0]), reinterpret_cast<const std::uint8_t *>(&effect.parameters[i].data.back()) + 1);
            }

            if (to_pad != 0) {
                const std::uint8_t padding_value = 0xE9;
                effects.insert(effects.end(), to_pad, padding_value);
            }

            srv_effect = reinterpret_cast<decltype(srv_effect)>(&effects[0]);
            srv_effect->effect_size_ += parameter.param_length_ + sizeof(akns_srv_effect_parameter_def);
        }
        return true;
    }

    bool akn_skin_chunk_maintainer::import_effect_queue(const skn_effect_queue &queue) {
        akns_item_def item;
        item.type_ = akns_item_type_effect_queue;
        item.id_ = make_pid_from_id_hash(queue.id_hash);

        if (queue.ref_major && queue.ref_minor) {
            akns_srv_effect_queue_def queue_def;
            queue_def.size_ = sizeof(akns_srv_effect_queue_def);
            queue_def.count_ = 0;
            queue_def.ref_major_ = queue.ref_major;
            queue_def.ref_minor_ = queue.ref_minor;
            return update_definition(item, &queue_def, sizeof(akns_item_type_effect_queue), 0xFFFFFFFF);
        }

        std::vector<std::uint8_t> buffer;
        buffer.resize(sizeof(akns_srv_effect_queue_def));
        akns_srv_effect_queue_def *queue_def = reinterpret_cast<akns_srv_effect_queue_def *>(&buffer[0]);

        queue_def->count_ = static_cast<std::uint32_t>(queue.effects.size());
        queue_def->ref_major_ = 0;
        queue_def->ref_minor_ = 0;
        queue_def->input_layer_index_ = queue.input_layer_index;
        queue_def->input_layer_mode_ = queue.input_layer_mode;
        queue_def->output_layer_index_ = queue.output_layer_index;
        queue_def->output_layer_mode_ = queue.output_layer_mode;
        queue_def->size_ = sizeof(akns_srv_effect_queue_def);

        std::vector<std::uint8_t> temp_effect_data;
        for (std::size_t i = 0; i < queue.effects.size(); i++) {
            if (!import_effect(*this, queue.effects[i], temp_effect_data))
                return false;
            queue_def->size_ += static_cast<std::uint32_t>(temp_effect_data.size());
            buffer.insert(buffer.end(), temp_effect_data.begin(), temp_effect_data.end());
            queue_def = reinterpret_cast<akns_srv_effect_queue_def *>(&buffer[0]);
            temp_effect_data.clear();
        }
        return update_definition(item, &buffer[0], buffer.size(), -1);
    }

    bool akn_skin_chunk_maintainer::import_bitmap(const skn_bitmap_info &info) {
        akns_item_def item;
        item.type_ = akns_item_type_bitmap;
        item.id_ = make_pid_from_id_hash(info.id_hash);

        if (info.mask_bitmap_idx == -1) {
            akns_srv_bitmap_def bitmap_def;
            bitmap_def.filename_.type_ = akns_mtptr_type_relative_ram;
            bitmap_def.filename_.address_or_offset_ = get_filename_offset_from_id(info.filename_id);
            bitmap_def.index_ = info.bmp_idx;
            bitmap_def.image_alignment_ = info.attrib.align;
            bitmap_def.image_attrib_ = info.attrib.attrib;
            bitmap_def.image_height_ = info.attrib.image_size_y;
            bitmap_def.image_width_ = info.attrib.image_size_x;
            bitmap_def.image_x_coord_ = info.attrib.image_coord_x;
            bitmap_def.image_y_coord_ = info.attrib.image_coord_y;
            return update_definition(item, &bitmap_def, sizeof(bitmap_def), sizeof(bitmap_def));
        }

        item.type_ = akns_item_type_masked_bitmap;
        akns_srv_masked_bitmap_def masked_bitmap_def;
        masked_bitmap_def.filename_.type_ = akns_mtptr_type_relative_ram;
        masked_bitmap_def.filename_.address_or_offset_ = get_filename_offset_from_id(info.filename_id);
        masked_bitmap_def.index_ = info.bmp_idx;
        masked_bitmap_def.masked_index_ = info.mask_bitmap_idx;
        masked_bitmap_def.image_alignment_ = info.attrib.align;
        masked_bitmap_def.image_attrib_ = info.attrib.attrib;
        masked_bitmap_def.image_height_ = info.attrib.image_size_y;
        masked_bitmap_def.image_width_ = info.attrib.image_size_x;
        masked_bitmap_def.image_x_coord_ = info.attrib.image_coord_x;
        masked_bitmap_def.image_y_coord_ = info.attrib.image_coord_y;

        return update_definition(item, &masked_bitmap_def, sizeof(masked_bitmap_def), sizeof(masked_bitmap_def));
    }

    bool akn_skin_chunk_maintainer::import_image_table(const skn_image_table &table) {
        akns_item_def item;
        item.type_ = akns_item_type_image_table;
        item.id_ = make_pid_from_id_hash(table.id_hash);

        akns_srv_image_table_def image_table;
        image_table.count_ = static_cast<std::int32_t>(table.images.size());
        image_table.entries_.type_ = akns_mtptr_type::akns_mtptr_type_relative_ram;

        akns_item_def *last_image_table_item = get_item_definition(item.id_);
        std::uint8_t *data_area = reinterpret_cast<std::uint8_t *>(get_area_base(akn_skin_chunk_area_base_offset::data_area_base));
        std::uint8_t *old_image_entries_data = nullptr;
        std::size_t old_size = 0;

        if (last_image_table_item) {
            akns_srv_color_table_def *last_image_table = last_image_table_item->data_.get_relative<akns_srv_image_table_def>(data_area);
            if (last_image_table) {
                old_image_entries_data = last_image_table->entries_.get_relative<std::uint8_t>(data_area);
                if (last_image_table->count_ > image_table.count_)
                    image_table.count_ = last_image_table->count_;
                old_size = last_image_table->count_ * sizeof(pid);
            }
        }

        image_table.entries_.type_ = akns_mtptr_type_relative_ram;
        image_table.entries_.address_or_offset_ = update_data(nullptr, old_image_entries_data, image_table.count_ * sizeof(pid), old_size);

        pid *entries_to_fill = image_table.entries_.get_relative<pid>(data_area);
        std::size_t index_ite = 0;
        for (const auto &entry : table.images) {
            entries_to_fill[index_ite] = make_pid_from_id_hash(entry);
            index_ite++;
        }

        image_table.image_alignment_ = table.attrib.align;
        image_table.image_attrib_ = table.attrib.attrib;
        image_table.image_height_ = table.attrib.image_size_y;
        image_table.image_width_ = table.attrib.image_size_x;
        image_table.image_x_coord_ = table.attrib.image_coord_x;
        image_table.image_y_coord_ = table.attrib.image_coord_y;

        return update_definition(item, &image_table, sizeof(akns_srv_image_table_def), sizeof(akns_srv_image_table_def));
    }

    bool akn_skin_chunk_maintainer::import(skn_file &skn, const std::u16string &filename_base) {
        for (auto &filename : skn.filenames_) {
            if (!update_filename(filename.first, filename.second, filename_base))
                return false;
        }
        for (auto &bmp : skn.bitmaps_) {
            if (!import_bitmap(bmp.second))
                return false;
        }
        for (auto &table : skn.color_tabs_) {
            if (!import_color_table(table.second))
                return false;
        }
        for (auto &table : skn.img_tabs_) {
            if (!import_image_table(table.second))
                return false;
        }
        for (auto &effect_queue : skn.effect_queues_) {
            if (!import_effect_queue(effect_queue))
                return false;
        }
        return true;
    }
}