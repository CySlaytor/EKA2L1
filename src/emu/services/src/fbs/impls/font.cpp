#include <common/cvt.h>
#include <common/log.h>
#include <common/path.h>
#include <common/vecx.h>
#include <services/fbs/fbs.h>
#include <system/epoc.h>
#include <utils/err.h>
#include <vfs/vfs.h>

namespace eka2l1::epoc {
    void open_font_glyph_offset_array::init(fbscli *cli, const std::int32_t count) {
        offset_array_count = count;
        std::int32_t *offset_array_ptr = reinterpret_cast<std::int32_t *>(cli->server<fbs_server>()
                ->allocate_general_data_impl(count * sizeof(std::int32_t)));
        std::fill(offset_array_ptr, offset_array_ptr + count, 0);

        if (epoc::does_client_use_pointer_instead_of_offset(cli)) {
            offset_array_offset = static_cast<std::int32_t>(
                cli->server<fbs_server>()->host_ptr_to_guest_general_data(offset_array_ptr).ptr_address());
        } else {
            offset_array_offset = static_cast<std::int32_t>(reinterpret_cast<std::uint8_t *>(offset_array_ptr) - reinterpret_cast<std::uint8_t *>(this));
        }
    }

    std::int32_t *open_font_glyph_offset_array::pointer(fbscli *cli) {
        if (offset_array_offset == 0)
            return nullptr;
        fbs_server *serv = cli->server<fbs_server>();
        if (epoc::does_client_use_pointer_instead_of_offset(cli)) {
            return eka2l1::ptr<std::int32_t>(offset_array_offset).get(serv->get_system()->get_memory_system());
        }
        return reinterpret_cast<std::int32_t *>(reinterpret_cast<std::uint8_t *>(this) + offset_array_offset);
    }

    void *open_font_glyph_offset_array::get_glyph(fbscli *client, const std::int32_t idx) {
        if (idx < 0 || idx >= offset_array_count)
            return nullptr;
        std::int32_t *offset_array_ptr = pointer(client);
        if (offset_array_ptr[idx] == 0)
            return nullptr;
        fbs_server *serv = client->server<fbs_server>();
        if (epoc::does_client_use_pointer_instead_of_offset(client)) {
            return eka2l1::ptr<void>(offset_array_ptr[idx]).get(serv->get_system()->get_memory_system());
        }
        return reinterpret_cast<std::uint8_t *>(this) + offset_array_ptr[idx];
    }

    bool open_font_glyph_offset_array::set_glyph(fbscli *client, const std::int32_t idx, void *cache_entry) {
        if (idx < 0 || idx >= offset_array_count)
            return false;
        std::int32_t *offset_array_ptr = pointer(client);
        if (!offset_array_ptr)
            return false;
        if (epoc::does_client_use_pointer_instead_of_offset(client)) {
            offset_array_ptr[idx] = static_cast<std::int32_t>(
                client->server<fbs_server>()->host_ptr_to_guest_general_data(cache_entry).ptr_address());
        } else {
            offset_array_ptr[idx] = static_cast<std::int32_t>(reinterpret_cast<std::uint8_t *>(cache_entry) - reinterpret_cast<std::uint8_t *>(this));
        }
        return true;
    }

    bool open_font_glyph_offset_array::is_entry_empty(fbscli *cli, const std::int32_t idx) {
        if (idx < 0 || idx >= offset_array_count)
            return true;
        auto ptr = pointer(cli);
        if (!ptr)
            return false;
        return (ptr[idx] == 0);
    }

    open_font_session_cache_v3 *open_font_session_cache_list::get(fbscli *cli, const std::int32_t session_handle, const bool create) {
        const std::int32_t *offset_ptr = find(session_handle);
        fbs_server *serv = cli->server<fbs_server>();

        if (!offset_ptr) {
            if (create) {
                open_font_session_cache_v3 *cache = serv->allocate_general_data<open_font_session_cache_v3>();
                const std::int32_t offset = static_cast<std::int32_t>(reinterpret_cast<std::uint8_t *>(cache) - reinterpret_cast<std::uint8_t *>(this));
                cache->session_handle = session_handle;
                cache->random_seed = 0;
                cache->offset_array.init(cli, NORMAL_SESSION_CACHE_ENTRY_COUNT);
                add(session_handle, offset);
                return cache;
            } else {
                return nullptr;
            }
        }

        if (epoc::does_client_use_pointer_instead_of_offset(cli)) {
            return eka2l1::ptr<open_font_session_cache_v3>(*offset_ptr).get(serv->get_system()->get_memory_system());
        }
        return reinterpret_cast<open_font_session_cache_v3 *>(reinterpret_cast<std::uint8_t *>(this) + *offset_ptr);
    }

    open_font_session_cache_link *open_font_session_cache_link::get_or_create(fbscli *cli) {
        open_font_session_cache_link *current = this;
        fbs_server *serv = cli->server<fbs_server>();
        memory_system *mem = serv->get_system()->get_memory_system();

        do {
            if (!current->next)
                break;
            current = current->next.get(mem);
            auto current_cache = current->cache.get(mem);
            if (current_cache->session_handle == cli->connection_id_) {
                return current;
            }
        } while (true);

        auto next_link = serv->allocate_general_data<open_font_session_cache_link>();
        current->next = serv->host_ptr_to_guest_general_data(next_link).cast<epoc::open_font_session_cache_link>();

        open_font_session_cache_old *cache = serv->allocate_general_data<open_font_session_cache_old>();
        cache->session_handle = cli->connection_id_;
        cache->offset_array.init(cli, NORMAL_SESSION_CACHE_ENTRY_COUNT);
        cache->last_use_counter = 0;

        next_link->cache = serv->host_ptr_to_guest_general_data(cache).cast<epoc::open_font_session_cache_old>();
        return next_link;
    }

    bool open_font_session_cache_link::remove(fbscli *cli) {
        open_font_session_cache_link *previous = nullptr;
        open_font_session_cache_link *current = this;
        fbs_server *serv = cli->server<fbs_server>();
        memory_system *mem = serv->get_system()->get_memory_system();

        do {
            if (!current->next)
                break;
            previous = current;
            current = current->next.get(mem);
            auto current_cache = current->cache.get(mem);

            if (current_cache->session_handle == cli->connection_id_) {
                previous->next = current->next;
                if (serv->legacy_level() >= FBS_LEGACY_LEVEL_KERNEL_TRANSITION) {
                    current_cache->destroy<epoc::open_font_session_cache_entry_v1>(cli);
                } else {
                    current_cache->destroy<epoc::open_font_session_cache_entry_v2>(cli);
                }
                serv->free_general_data(current_cache);
                serv->free_general_data(current);
                return true;
            }
        } while (true);

        return false;
    }

    bool open_font_session_cache_list::erase_cache(fbscli *cli) {
        auto cache = get(cli, cli->connection_id_, false);
        if (cache) {
            cache->destroy(cli);
            cli->server<fbs_server>()->free_general_data(cache);
            this->remove(cli->connection_id_);
            return true;
        }
        return false;
    }

    void open_font_glyph_v2::destroy(fbscli *cli) {}
    void open_font_glyph_v3::destroy(fbscli *cli) {}

    void open_font_session_cache_v3::destroy(fbscli *cli) {
        for (std::int32_t i = 0; i < offset_array.offset_array_count; i++) {
            if (!offset_array.is_entry_empty(cli, i)) {
                auto glyph_cache = offset_array.get_glyph(cli, i);
                auto serv = cli->server<fbs_server>();
                reinterpret_cast<open_font_session_cache_entry_v3 *>(glyph_cache)->destroy(cli);
                serv->free_general_data(glyph_cache);
            }
        }
    }

    void open_font_session_cache_v3::add_glyph(fbscli *cli, const std::uint32_t code, void *the_glyph) {
        const std::uint32_t real_index = (code & 0x7fffffff) % offset_array.offset_array_count;
        if (!offset_array.is_entry_empty(cli, real_index)) {
            auto glyph_cache = offset_array.get_glyph(cli, real_index);
            reinterpret_cast<open_font_session_cache_entry_v3 *>(glyph_cache)->destroy(cli);
            cli->server<fbs_server>()->free_general_data(glyph_cache);
        }
        offset_array.set_glyph(cli, real_index, the_glyph);
    }

    template <typename T>
    void open_font_session_cache_old::destroy(fbscli *cli) {
        for (std::int32_t i = 0; i < offset_array.offset_array_count; i++) {
            if (!offset_array.is_entry_empty(cli, i)) {
                auto glyph_cache = offset_array.get_glyph(cli, i);
                auto serv = cli->server<fbs_server>();
                reinterpret_cast<T *>(glyph_cache)->destroy(cli);
                serv->free_general_data(glyph_cache);
            }
        }
    }

    template <typename T>
    void open_font_session_cache_old::add_glyph(fbscli *cli, const std::uint32_t code, void *the_glyph) {
        std::uint32_t real_index = (code & 0x7fffffff) % offset_array.offset_array_count;
        if (!offset_array.is_entry_empty(cli, real_index)) {
            auto ptr = offset_array.pointer(cli);
            fbs_server *serv = cli->server<fbs_server>();
            memory_system *mem = serv->get_system()->get_memory_system();
            std::int32_t least_use = 99999999;
            for (std::int32_t i = 0; i < offset_array.offset_array_count; i++) {
                if (!ptr[i]) {
                    real_index = i;
                    break;
                }
                auto entry = eka2l1::ptr<T>(ptr[i]).get(mem);
                if (entry->last_use < least_use) {
                    least_use = entry->last_use;
                    real_index = i;
                }
            }
        }

        if (!offset_array.is_entry_empty(cli, real_index)) {
            auto glyph_cache = offset_array.get_glyph(cli, real_index);
            reinterpret_cast<T *>(glyph_cache)->destroy(cli);
            cli->server<fbs_server>()->free_general_data(glyph_cache);
        }

        reinterpret_cast<T *>(the_glyph)->glyph_index = real_index;
        offset_array.set_glyph(cli, real_index, the_glyph);
    }

    template void open_font_session_cache_old::add_glyph<open_font_session_cache_entry_v1>(fbscli *cli, const std::uint32_t code, void *the_glyph);
    template void open_font_session_cache_old::add_glyph<open_font_session_cache_entry_v2>(fbscli *cli, const std::uint32_t code, void *the_glyph);
    template void open_font_session_cache_old::destroy<open_font_session_cache_entry_v1>(fbscli *cli);
    template void open_font_session_cache_old::destroy<open_font_session_cache_entry_v2>(fbscli *cli);

    open_font_glyph_cache_v1::open_font_glyph_cache_v1()
        : entry_(0)
        , unk4_(0)
        , unk8_(0) {}
}

namespace eka2l1 {
    static bool is_opcode_ruler_twips(const int opcode) {
        return (opcode == fbs_nearest_font_design_height_in_twips || opcode == fbs_nearest_font_max_height_in_twips);
    }

    struct font_info {
        std::int32_t handle;
        std::int32_t address_offset;
        std::int32_t server_handle;
    };

    fbsfont *fbs_server::look_for_font_with_address(const eka2l1::address addr) {
        const address base_shared_old_mm_model = shared_chunk->base(nullptr).ptr_address();
        for (auto &font_cache_obj_ptr : font_obj_container) {
            fbsfont *temp_font_ptr = reinterpret_cast<fbsfont *>(font_cache_obj_ptr.get());
            if (temp_font_ptr && (temp_font_ptr->guest_font_offset + base_shared_old_mm_model) == addr) {
                return temp_font_ptr;
            }
        }
        return nullptr;
    }

    static std::int32_t calculate_baseline(epoc::font_spec_base &spec) {
        if (static_cast<epoc::font_spec_v1 &>(spec).style.flags & epoc::font_style_base::super) {
            constexpr std::int32_t super_script_offset_percentage = -28;
            return super_script_offset_percentage * spec.height / 100;
        }
        if (static_cast<epoc::font_spec_v1 &>(spec).style.flags & epoc::font_style_base::sub) {
            constexpr std::int32_t subscript_offset_percentage = 14;
            return subscript_offset_percentage * spec.height / 100;
        }
        return 0;
    }

    static void calculate_algorithic_style(epoc::alg_style &style, epoc::font_spec_base &spec) {
        style.baseline_offsets_in_pixel = calculate_baseline(spec);
        style.height_factor = 1;
        style.width_factor = 1;
        style.flags = 0;
    }

    static void do_fill_bitmap_font_spec(epoc::font_spec_base &target_spec, epoc::font_spec_base &given_spec,
        std::int16_t adjusted_height, epoc::adapter::font_file_adapter_base *adapter) {
        target_spec.height = adjusted_height * 15;
        static_cast<epoc::font_spec_v1 &>(target_spec).style.reset_flags();
        static_cast<epoc::font_spec_v1 &>(target_spec).style.set_glyph_bitmap_type(adapter->get_output_bitmap_type());
    }

    void fbscli::num_typefaces(service::ipc_context *ctx) {
        ctx->complete(static_cast<std::int32_t>(server<fbs_server>()->persistent_font_store.number_of_typefaces()));
    }

    void fbscli::typeface_support(service::ipc_context *ctx) {
        std::optional<std::uint32_t> font_idx = ctx->get_argument_value<std::uint32_t>(0);
        std::optional<epoc::typeface_support> support = ctx->get_argument_data_from_descriptor<epoc::typeface_support>(1);

        if (!support || !font_idx) {
            ctx->complete(epoc::error_argument);
            return;
        }

        fbs_server *serv = server<fbs_server>();
        epoc::typeface_support *support_stored = serv->persistent_font_store.get_typeface_support(font_idx.value());

        if (!support_stored) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        const epocver ver_sym = ctx->sys->get_symbian_version_use();
        support.value() = *support_stored;
        support->min_height_in_twips_ *= epoc::get_approximate_pixel_to_twips_mul(ver_sym);
        support->max_height_in_twips_ *= epoc::get_approximate_pixel_to_twips_mul(ver_sym);

        ctx->write_data_to_descriptor_argument(1, support.value());
        ctx->complete(epoc::error_none);
    }

    template <typename T, typename Q>
    void fbscli::fill_bitmap_information(T *bmpfont, Q *of, epoc::open_font_info &info, epoc::font_spec_base &spec, kernel::process *font_user) {
        fbs_server *serv = server<fbs_server>();
        if (epoc::does_client_use_pointer_instead_of_offset(this)) {
            bmpfont->openfont = serv->host_ptr_to_guest_general_data(of).template cast<void>();
        } else {
            bmpfont->openfont = static_cast<std::int32_t>(reinterpret_cast<std::uint8_t *>(of) - reinterpret_cast<std::uint8_t *>(bmpfont)) | 0x1;
        }

        bmpfont->vtable = serv->fntstr_seg->relocate(font_user, serv->bmp_font_vtab.ptr_address());
        calculate_algorithic_style(bmpfont->algorithic_style, spec);
        do_fill_bitmap_font_spec(bmpfont->spec_in_twips, spec, info.metrics.design_height, info.adapter);

        bmpfont->spec_in_twips.tf.name.assign(nullptr, info.face_attrib.fam_name.to_std_string(nullptr));
        bmpfont->spec_in_twips.tf.flags = spec.tf.flags;

        of->metrics = info.metrics;
        of->face_index_offset = static_cast<int>(info.idx);
        of->vtable = epoc::DEAD_VTABLE;
        of->allocator = epoc::DEAD_ALLOC;

        if constexpr (std::is_same_v<Q, epoc::open_font_v2>) {
            of->font_max_ascent = of->metrics.ascent;
            of->font_max_descent = of->metrics.descent;
            of->font_standard_descent = of->metrics.descent;
            of->font_captial_offset = 0;
            of->font_line_gap = static_cast<std::uint16_t>(info.adapter->line_gap(info.idx, info.metric_identifier));
        }

        if (epoc::does_client_use_pointer_instead_of_offset(this)) {
            of->session_cache_list_offset = static_cast<std::int32_t>(serv->host_ptr_to_guest_general_data(serv->session_cache_link).ptr_address());
        } else {
            of->session_cache_list_offset = static_cast<std::int32_t>(reinterpret_cast<std::uint8_t *>(serv->session_cache_list) - reinterpret_cast<std::uint8_t *>(of));
        }

        if (serv->kern->is_eka1()) {
            epoc::open_font_glyph_cache_v1 *cache = serv->allocate_general_data<epoc::open_font_glyph_cache_v1>();
            if (!cache) {
                LOG_WARN(SERVICE_FBS, "Unable to supply empty cache for EKA1's open font!");
            } else {
                cache->entry_ = 0;
                of->glyph_cache_offset = static_cast<std::int32_t>(serv->host_ptr_to_guest_general_data(cache).ptr_address());
            }
        } else {
            of->glyph_cache_offset = 0;
        }
    }

    template <typename T>
    void fbs_server::destroy_bitmap_font(T *bmpfont) {
        if (legacy_level() >= FBS_LEGACY_LEVEL_KERNEL_TRANSITION) {
            epoc::open_font_v1 *ofo = reinterpret_cast<epoc::open_font_v1 *>(guest_general_data_to_host_ptr(bmpfont->openfont.template cast<std::uint8_t>()));
            if (ofo->glyph_cache_offset)
                free_general_data_impl(guest_general_data_to_host_ptr(ofo->glyph_cache_offset));
        }

        if (bmpfont->openfont.ptr_address() & 0x1) {
            free_general_data_impl(reinterpret_cast<std::uint8_t *>(bmpfont) + (bmpfont->openfont.ptr_address() & ~0x1));
        } else {
            free_general_data_impl(guest_general_data_to_host_ptr(bmpfont->openfont.template cast<std::uint8_t>()));
        }
        free_general_data(bmpfont);
    }

    template void fbscli::fill_bitmap_information<epoc::bitmapfont_v1, epoc::open_font_v1>(epoc::bitmapfont_v1 *bitmapfont, epoc::open_font_v1 *of, epoc::open_font_info &info, epoc::font_spec_base &spec, kernel::process *font_user);
    template void fbscli::fill_bitmap_information<epoc::bitmapfont_v2, epoc::open_font_v2>(epoc::bitmapfont_v2 *bitmapfont, epoc::open_font_v2 *of, epoc::open_font_info &info, epoc::font_spec_base &spec, kernel::process *font_user);

    template void fbs_server::destroy_bitmap_font<epoc::bitmapfont_v1>(epoc::bitmapfont_v1 *bmpfont);
    template void fbs_server::destroy_bitmap_font<epoc::bitmapfont_v2>(epoc::bitmapfont_v2 *bmpfont);

    epoc::bitmapfont_base *fbscli::create_bitmap_open_font(epoc::open_font_info &info, epoc::font_spec_base &spec, kernel::process *font_user) {
        fbs_server *serv = server<fbs_server>();
#define DO_BITMAP_OPEN_FONT_CREATION(ver)                                                      \
    epoc::open_font_v##ver *of = serv->allocate_general_data<epoc::open_font_v##ver>();        \
    if (!of)                                                                                   \
        return nullptr;                                                                        \
    epoc::bitmapfont_v##ver *bmpfont = serv->allocate_general_data<epoc::bitmapfont_v##ver>(); \
    if (!bmpfont) {                                                                            \
        serv->free_general_data(of);                                                           \
        return nullptr;                                                                        \
    }                                                                                          \
    fill_bitmap_information<epoc::bitmapfont_v##ver>(bmpfont, of, info, spec, font_user);      \
    return bmpfont

        if (serv->kern->is_eka1()) {
            DO_BITMAP_OPEN_FONT_CREATION(1);
        }
        DO_BITMAP_OPEN_FONT_CREATION(2);
    }

    void fbscli::write_font_handle(service::ipc_context *ctx, fbsfont *font, const int index) {
        font_info result_info;
        result_info.handle = obj_table_.add(font);
        result_info.address_offset = font->guest_font_offset;
        result_info.server_handle = static_cast<std::int32_t>(font->id);
        ctx->write_data_to_descriptor_argument(index, result_info);
        ctx->complete(epoc::error_none);
    }

    void fbscli::get_nearest_font(service::ipc_context *ctx) {
        epoc::font_spec_v1 spec = *ctx->get_argument_data_from_descriptor<epoc::font_spec_v1>(0);
        std::optional<eka2l1::vec3> size_info = ctx->get_argument_data_from_descriptor<eka2l1::vec3>(2);
        fbs_server *serv = server<fbs_server>();

        const bool is_twips = is_opcode_ruler_twips(ctx->msg->function);
        const bool is_design_height = (serv->kern->is_eka1() || (!size_info.has_value()) || (ctx->msg->function == fbs_nearest_font_design_height_in_twips) || (ctx->msg->function == fbs_nearest_font_design_height_in_pixels));

        if (serv->kern->is_eka1() || is_twips) {
            spec.height = static_cast<std::int32_t>(static_cast<float>(spec.height) / epoc::get_approximate_pixel_to_twips_mul(serv->kern->get_epoc_version()));
            size_info->x = static_cast<std::int32_t>(static_cast<float>(size_info->x) / epoc::get_approximate_pixel_to_twips_mul(serv->kern->get_epoc_version()));
        }

        static constexpr std::int32_t MAX_FONT_HEIGHT = 256;
        static constexpr std::int32_t MIN_FONT_HEIGHT = 2;
        spec.height = common::clamp<std::int32_t>(MIN_FONT_HEIGHT, MAX_FONT_HEIGHT, spec.height);

        if (spec.tf.name.get_length() == 0) {
            spec.tf.name = serv->default_system_font;
        }

        fbsfont *font = nullptr;
        epoc::open_font_info *ofi_suit = serv->persistent_font_store.seek_the_open_font(spec);

        if (!ofi_suit) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        static constexpr int max_acceptable_delta = 1;
        for (auto &font_obj : server<fbs_server>()->font_obj_container) {
            fbsfont *the_font = reinterpret_cast<fbsfont *>(font_obj.get());
            const std::int32_t delta = common::abs(is_design_height ? (spec.height - the_font->of_info.metrics.design_height) : (size_info->x - the_font->of_info.metrics.max_height));

            if ((the_font->of_info.face_attrib.name.to_std_string(nullptr) == ofi_suit->face_attrib.name.to_std_string(nullptr))
                && (delta <= max_acceptable_delta)) {
                font = the_font;
                break;
            }
        }

        if (!font) {
            font = serv->font_obj_container.make_new<fbsfont>();
            font->serv = serv;
            font->of_info = *ofi_suit;
            font->of_info.metrics = ofi_suit->adapter->get_nearest_supported_metric(ofi_suit->idx,
                                                         is_design_height ? static_cast<std::uint16_t>(spec.height) : static_cast<std::uint16_t>(size_info->x),
                                                         &font->of_info.metric_identifier, is_design_height)
                                        .value();
            epoc::bitmapfont_base *bmpfont = create_bitmap_open_font(font->of_info, spec, ctx->msg->own_thr->owning_process());
            if (!bmpfont) {
                ctx->complete(epoc::error_no_memory);
                return;
            }
            font->guest_font_offset = serv->host_ptr_to_guest_shared_offset(bmpfont);
        }

        write_font_handle(ctx, font, 1);
    }

    void fbscli::duplicate_font(service::ipc_context *ctx) {
        fbs_server *serv = server<fbs_server>();
        fbsfont *font = serv->font_obj_container.get<fbsfont>(*ctx->get_argument_value<epoc::handle>(0));
        if (!font) {
            ctx->complete(epoc::error_not_found);
            return;
        }
        write_font_handle(ctx, font, 1);
    }

    fbsfont *fbscli::get_font_object(service::ipc_context *ctx) {
        if ((ver_.build > 94) || (server<fbs_server>()->get_system()->get_symbian_version_use() >= epocver::epoc95)) {
            return obj_table_.get<fbsfont>(*ctx->get_argument_value<epoc::handle>(0));
        }
        const eka2l1::address addr = *ctx->get_argument_value<eka2l1::address>(0);
        return server<fbs_server>()->look_for_font_with_address(addr);
    }

    void fbscli::rasterize_glyph(service::ipc_context *ctx) {
        const std::uint32_t codepoint = *ctx->get_argument_value<std::uint32_t>(1);
        const fbsfont *font = get_font_object(ctx);
        process_ptr own_pr = ctx->msg->own_thr->owning_process();
        epoc::bitmapfont_base *bmp_font = reinterpret_cast<epoc::bitmapfont_base *>(font->guest_font_offset + reinterpret_cast<std::uint8_t *>(server<fbs_server>()->shared_chunk->host_base()));

        int rasterized_width = 0;
        int rasterized_height = 0;
        const epoc::open_font_info *info = &(font->of_info);
        epoc::glyph_bitmap_type bitmap_type = epoc::glyph_bitmap_type::default_glyph_bitmap;
        std::uint32_t bitmap_data_size = 0;
        epoc::open_font_character_metric char_metric;

        std::uint8_t *bitmap_data = info->adapter->get_glyph_bitmap(info->idx, codepoint, font->of_info.metric_identifier,
            &rasterized_width, &rasterized_height, bitmap_data_size, &bitmap_type, char_metric);

        if (!bitmap_data && !info->adapter->does_glyph_exist(info->idx, codepoint, info->metric_identifier)) {
            ctx->complete(0);
            return;
        }

        fbs_server *serv = server<fbs_server>();
        kernel::process *pr = ctx->msg->own_thr->owning_process();

#define MAKE_CACHE_ENTRY(entry_ver, type)                                                                                                   \
    epoc::open_font_session_cache_entry_v##entry_ver *cache_entry = reinterpret_cast<decltype(cache_entry)>(                                \
        serv->allocate_general_data_impl(sizeof(epoc::open_font_session_cache_entry_v##entry_ver) + bitmap_data_size + 1));                 \
    cache_entry->codepoint = codepoint;                                                                                                     \
    cache_entry->glyph_index = codepoint % session_cache->offset_array.offset_array_count;                                                  \
    cache_entry->offset = sizeof(epoc::open_font_session_cache_entry_v##entry_ver) + 1;                                                     \
    cache_entry->metric = char_metric;                                                                                                      \
    cache_entry->metric.bitmap_type = bitmap_type;                                                                                          \
    const auto cache_entry_ptr = serv->host_ptr_to_guest_general_data(cache_entry).ptr_address();                                           \
    if (epoc::does_client_use_pointer_instead_of_offset(this)) {                                                                            \
        cache_entry->font_offset = static_cast<std::int32_t>(reinterpret_cast<type *>(bmp_font)->openfont.ptr_address());                   \
    } else {                                                                                                                                \
        cache_entry->font_offset = static_cast<std::int32_t>(reinterpret_cast<type *>(bmp_font)->openfont.ptr_address() - cache_entry_ptr); \
    }                                                                                                                                       \
    std::memcpy(reinterpret_cast<std::uint8_t *>(cache_entry) + cache_entry->offset, bitmap_data,                                           \
        bitmap_data_size);                                                                                                                  \
    info->adapter->free_glyph_bitmap(bitmap_data);                                                                                          \
    if (epoc::does_client_use_pointer_instead_of_offset(this)) {                                                                            \
        cache_entry->offset += static_cast<std::int32_t>(cache_entry_ptr);                                                                  \
    }

        if (epoc::does_client_use_pointer_instead_of_offset(this)) {
            epoc::open_font_session_cache_link *link = serv->session_cache_link->get_or_create(this);
            epoc::open_font_session_cache_old *session_cache = link->cache.get(pr);

            if (serv->kern->is_eka1()) {
                MAKE_CACHE_ENTRY(1, epoc::bitmapfont_v1);
                session_cache->add_glyph<epoc::open_font_session_cache_entry_v1>(this, codepoint, cache_entry);
                cache_entry->last_use = session_cache->last_use_counter++;
                glyph_info_for_legacy_return_->codepoint = codepoint;
                glyph_info_for_legacy_return_->metric_offset = serv->host_ptr_to_guest_general_data(&cache_entry->metric).ptr_address();
                glyph_info_for_legacy_return_->offset = cache_entry->offset;
                ctx->complete(glyph_info_for_legacy_return_addr_);
                return;
            } else {
                MAKE_CACHE_ENTRY(2, epoc::bitmapfont_v2);
                session_cache->add_glyph<epoc::open_font_session_cache_entry_v2>(this, codepoint, cache_entry);
                cache_entry->last_use = session_cache->last_use_counter++;
                cache_entry->metric_offset = serv->host_ptr_to_guest_general_data(&cache_entry->metric).ptr_address();
                ctx->complete(cache_entry_ptr);
                return;
            }
        }

        epoc::open_font_session_cache_v3 *session_cache = serv->session_cache_list->get(this, static_cast<std::int32_t>(connection_id_), true);
        MAKE_CACHE_ENTRY(3, epoc::bitmapfont_v2);
        session_cache->add_glyph(this, codepoint, cache_entry);

        struct rasterize_param {
            std::int32_t metrics_offset;
            std::int32_t bitmap_offset;
        };

        if (*ctx->get_argument_value<std::int32_t>(2) != 0) {
            rasterize_param param;
            param.metrics_offset = static_cast<std::int32_t>(serv->host_ptr_to_guest_shared_offset(&cache_entry->metric));
            param.bitmap_offset = static_cast<std::int32_t>(serv->host_ptr_to_guest_shared_offset(
                reinterpret_cast<std::uint8_t *>(cache_entry) + cache_entry->offset));
            ctx->write_data_to_descriptor_argument<rasterize_param>(2, param);
        }
        ctx->complete(true);
    }

    void fbscli::set_default_glyph_bitmap_type(service::ipc_context *ctx) {
        std::optional<std::uint32_t> default_type = ctx->get_argument_value<std::uint32_t>(0);
        if (!default_type.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }
        server<fbs_server>()->set_default_glyph_bitmap_type(static_cast<epoc::glyph_bitmap_type>(default_type.value()));
        ctx->complete(epoc::error_none);
    }

    fbsfont::~fbsfont() {
        atlas.destroy(serv->get_graphics_driver());
        std::uint8_t *font_ptr = serv->get_shared_chunk_base() + guest_font_offset;

        switch (serv->legacy_level()) {
        case FBS_LEGACY_LEVEL_KERNEL_TRANSITION:
        case FBS_LEGACY_LEVEL_EARLY_KERNEL_TRANSITION:
        case FBS_LEGACY_LEVEL_S60V1:
            serv->destroy_bitmap_font<epoc::bitmapfont_v1>(reinterpret_cast<epoc::bitmapfont_v1 *>(font_ptr));
            break;
        default:
            serv->destroy_bitmap_font<epoc::bitmapfont_v2>(reinterpret_cast<epoc::bitmapfont_v2 *>(font_ptr));
            break;
        }
        for (std::size_t i = 0; i < shapings.size(); i++) {
            serv->free_general_data_impl(shapings[i]);
        }
    }

    void fbs_server::load_fonts_from_directory(eka2l1::io_system *io, eka2l1::directory *folder) {
        while (auto entry = folder->get_next_entry()) {
            add_single_font(io, common::utf8_to_ucs2(entry->full_path));
        }
    }

    bool fbs_server::add_single_font(eka2l1::io_system *io, const std::u16string &path) {
        symfile f = io->open_file(path, READ_MODE | BIN_MODE);
        const std::uint64_t fsize = f->size();

        std::vector<std::uint8_t> buf;
        buf.resize(fsize);
        f->read_file(&buf[0], 1, static_cast<std::uint32_t>(buf.size()));
        f->close();

        const auto extension = common::lowercase_string(eka2l1::path_extension(common::ucs2_to_utf8(path)));
        epoc::adapter::font_file_adapter_kind adapter_kind = epoc::adapter::font_file_adapter_kind::none;

        if (extension == ".ttf") {
            adapter_kind = epoc::adapter::font_file_adapter_kind::freetype;
        }

        if (adapter_kind != epoc::adapter::font_file_adapter_kind::none) {
            persistent_font_store.add_fonts(buf, adapter_kind);
        }
        return true;
    }

    void fbs_server::load_fonts(eka2l1::io_system *io) {
        for (drive_number drv = drive_z; drv >= drive_a; drv = static_cast<drive_number>(static_cast<int>(drv) - 1)) {
            if (io->get_drive_entry(drv)) {
                const std::u16string fonts_folder_path = std::u16string{ drive_to_char16(drv) } + (kern->is_eka1() ? u":\\System\\Fonts\\" : u":\\Resource\\Fonts\\");
                auto folder = io->open_dir(fonts_folder_path, {}, io_attrib_include_file);

                if (folder) {
                    LOG_TRACE(SERVICE_FBS, "Found font folder: {}", common::ucs2_to_utf8(fonts_folder_path));
                    load_fonts_from_directory(io, folder.get());
                }
            }
        }
    }
}