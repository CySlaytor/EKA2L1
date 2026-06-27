#include <services/centralrepo/centralrepo.h>
#include <services/centralrepo/repo.h>
#include <services/context.h>
#include <system/epoc.h>
#include <utils/err.h>

#include <common/cvt.h>
#include <system/devices.h>

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace eka2l1 {
    std::uint32_t central_repo::get_default_meta_for_new_key(const std::uint32_t key) {
        for (std::size_t i = 0; i < meta_range.size(); i++) {
            if (meta_range[i].high_key) {
                if (meta_range[i].low_key <= key
                    && key <= meta_range[i].high_key) {
                    return meta_range[i].default_meta_data;
                }
            } else {
                if ((meta_range[i].key_mask & key)
                    == (meta_range[i].low_key & key)) {
                    return meta_range[i].default_meta_data;
                }
            }
        }

        return default_meta;
    }

    bool central_repo::add_new_entry(const std::uint32_t key, const central_repo_entry_variant &var) {
        if (find_entry(key)) {
            return false;
        }

        central_repo_entry entry;
        entry.metadata_val = get_default_meta_for_new_key(key);
        entry.key = key;
        entry.data = var;

        entries.push_back(entry);

        return true;
    }

    bool central_repo::add_new_entry(const std::uint32_t key, const central_repo_entry_variant &var,
        const std::uint32_t meta) {
        if (find_entry(key)) {
            return false;
        }

        central_repo_entry entry;
        entry.metadata_val = meta;
        entry.key = key;
        entry.data = var;

        entries.push_back(entry);

        return true;
    }

    central_repo_entry *central_repo::find_entry(const std::uint32_t key) {
        auto ite = std::find_if(entries.begin(), entries.end(), [=](const central_repo_entry &e) {
            return e.key == key;
        });

        if (ite == entries.end()) {
            return nullptr;
        }

        return &(*ite);
    }

    void central_repo::query_entries(const std::uint32_t partial_key, const std::uint32_t mask,
        std::vector<central_repo_entry *> &matched_entries,
        const central_repo_entry_type etype) {
        std::uint32_t required_mask = mask & partial_key;

        for (auto &entry : entries) {
            if ((entry.key & required_mask) && (entry.data.etype == etype)) {
                matched_entries.push_back(&entry);
            }
        }
    }

    central_repo_client_subsession::central_repo_client_subsession()
        : flags(0) {
        set_transaction_mode(central_repo_transaction_mode::read_write);
    }

    void central_repo_client_subsession::modification_success(const std::uint32_t key) {
        for (std::size_t i = 0; i < notifies.size(); i++) {
            cenrep_notify_info &notify = notifies[i];

            if ((key & (notify.match & notify.mask))) {
                notify.sts.complete(0);
                notifies.erase(notifies.begin() + i);
            }
        }
    }

    int central_repo_client_subsession::add_notify_request(epoc::notify_info &info,
        const std::uint32_t mask, const std::uint32_t match) {
        auto find_result = std::find_if(notifies.begin(), notifies.end(), [&](const cenrep_notify_info &notify) {
            return (notify.mask == mask) && (notify.match == match);
        });

        if (find_result != notifies.end()) {
            return -1;
        } else {
            if (info.empty()) {
                return 0;
            }
        }

        notifies.push_back({ info, mask, match });
        return 0;
    }

    void central_repo_client_subsession::cancel_all_notify_requests() {
        common::erase_elements(notifies, [=](cenrep_notify_info &info) {
            info.sts.complete(-3);
            return true;
        });
    }

    void central_repo_client_subsession::cancel_notify_request(const std::uint32_t match_key, const std::uint32_t mask) {
        common::erase_elements(notifies, [=](cenrep_notify_info &info) {
            if ((info.match == match_key) && (info.mask == mask)) {
                info.sts.complete(-3);
                return true;
            }

            return false;
        });
    }

    central_repo_entry *central_repo_client_subsession::get_entry(const std::uint32_t key, int mode) {
        bool active = is_active();

        if ((mode == 1) && (get_transaction_mode() == central_repo_transaction_mode::read_only)) {
            return nullptr;
        }

        if (active) {
            auto entry_ite = transactor.changes.find(key);
            if (entry_ite != transactor.changes.end()) {
                return &(entry_ite->second);
            }
        }

        if (!active || mode == 0) {
            auto result = std::find_if(attach_repo->entries.begin(), attach_repo->entries.end(),
                [&](const central_repo_entry &entry) { return entry.key == key; });

            if (result == attach_repo->entries.end()) {
                return nullptr;
            }

            return &(*result);
        }

        transactor.changes.emplace(key, central_repo_entry{});
        return &(transactor.changes[key]);
    }

    void central_repo_client_subsession::reset(service::ipc_context *ctx) {
        // Only basic logic
        ctx->complete(epoc::error_not_supported);
    }

    void central_repo_client_subsession::create_value(service::ipc_context *ctx) {
        std::optional<std::uint32_t> id = ctx->get_argument_value<std::uint32_t>(0);

        if (!id) {
            ctx->complete(epoc::error_argument);
            return;
        }

        central_repo_entry_variant new_var;
        bool hit = true;

        switch (ctx->msg->function) {
        case cen_rep_create_int:
            new_var.etype = central_repo_entry_type::integer;
            new_var.intd = ctx->msg->args.args[1];

            break;

        case cen_rep_create_real: {
            new_var.etype = central_repo_entry_type::real;
            std::optional<double> dd_val = ctx->get_argument_data_from_descriptor<double>(1);

            if (!dd_val.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            new_var.reald = dd_val.value();
            break;
        }

        case cen_rep_create_string: {
            new_var.etype = central_repo_entry_type::string;
            std::optional<std::string> bb_val = ctx->get_argument_value<std::string>(1);

            if (!bb_val.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            new_var.strd.resize(bb_val->length());
            std::memcpy(new_var.strd.data(), bb_val->data(), bb_val->size());

            break;
        }

        default:
            hit = false;
            break;
        }

        if (!hit) {
            LOG_ERROR(SERVICE_CENREP, "Unknown create value type!");
            ctx->complete(epoc::error_general);
            return;
        }

        if (!attach_repo->add_new_entry(id.value(), new_var)) {
            LOG_ERROR(SERVICE_CENREP, "Fail to add new entry with id {}", id.value());
            ctx->complete(epoc::error_general);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    void central_repo_client_subsession::set_value(service::ipc_context *ctx) {
        central_repo_entry *entry = get_entry(*ctx->get_argument_value<std::uint32_t>(0), 1);

        if (!entry) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        switch (ctx->msg->function) {
        case cen_rep_set_int: {
            if (entry->data.etype != central_repo_entry_type::integer) {
                ctx->complete(epoc::error_argument);
                break;
            }

            entry->data.intd = static_cast<std::uint64_t>(*ctx->get_argument_value<std::uint32_t>(1));
            break;
        }

        case cen_rep_set_real: {
            if (entry->data.etype != central_repo_entry_type::real) {
                ctx->complete(epoc::error_argument);
                break;
            }

            std::optional<double> data = ctx->get_argument_data_from_descriptor<double>(1);
            if (!data.has_value()) {
                ctx->complete(epoc::error_argument);
                break;
            }

            entry->data.reald = data.value();
            break;
        }

        case cen_rep_set_string: {
            if (entry->data.etype != central_repo_entry_type::string) {
                ctx->complete(epoc::error_argument);
                break;
            }

            entry->data.strd = *ctx->get_argument_value<std::string>(1);
            break;
        }

        default: {
            assert(false);
            break;
        }
        }

        modification_success(entry->key);
        ctx->complete(epoc::error_none);
    }

#ifdef _MSC_VER
#pragma optimize("", off)
#endif
    void central_repo_client_subsession::get_value(service::ipc_context *ctx) {
        std::optional<std::uint32_t> the_key = ctx->get_argument_value<std::uint32_t>(0);

        if (!the_key.has_value()) {
            ctx->complete(epoc::error_argument);
            return;
        }

        central_repo_entry *entry = get_entry(the_key.value(), 0);

        if (!entry) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        switch (ctx->msg->function) {
        case cen_rep_get_int: {
            if (entry->data.etype != central_repo_entry_type::integer) {
                ctx->complete(epoc::error_argument);
                return;
            }

            const std::uint32_t result_int = static_cast<std::uint32_t>(entry->data.intd);
            ctx->write_data_to_descriptor_argument<std::uint32_t>(1, result_int);

            break;
        }

        case cen_rep_get_real: {
            if (entry->data.etype != central_repo_entry_type::real) {
                ctx->complete(epoc::error_argument);
                return;
            }

            const double result_fl = static_cast<double>(entry->data.reald);
            ctx->write_data_to_descriptor_argument<double>(1, result_fl);

            break;
        }

        case cen_rep_get_string: {
            if (entry->data.etype == central_repo_entry_type::string) {
                std::optional<std::uint32_t> wanted_length = ctx->get_argument_data_from_descriptor<std::uint32_t>(2);
                if (wanted_length.has_value()) {
                    wanted_length.value() = static_cast<std::uint32_t>(entry->data.strd.length());
                    ctx->write_data_to_descriptor_argument<std::uint32_t>(2, wanted_length.value());
                }

                const std::size_t buffer_length = ctx->get_argument_max_data_size(1);

                ctx->write_data_to_descriptor_argument(1, reinterpret_cast<std::uint8_t *>(&entry->data.strd[0]),
                    static_cast<std::uint32_t>(common::min(entry->data.strd.length(), buffer_length)));

                if (buffer_length < entry->data.strd.length()) {
                    ctx->complete(epoc::error_overflow);
                    return;
                }
            } else {
                ctx->complete(epoc::error_argument);
                return;
            }

            break;
        }

        default:
            LOG_ERROR(SERVICE_CENREP, "Invalid cenrep entry get type!");
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->complete(epoc::error_none);
    }
#ifdef _MSC_VER
#pragma optimize("", on)
#endif

    void central_repo_client_subsession::append_new_key_to_found_eq_list(std::uint32_t *array, const std::uint32_t key, const std::uint32_t max_uids_buf) {
        array[0]++;

        if (array[0] <= max_uids_buf) {
            array[array[0]] = key;
        } else {
            key_found_result.push_back(key);
        }
    }

    void central_repo_client_subsession::find(service::ipc_context *ctx) {
        key_found_result.clear();

        std::optional<central_repo_key_filter> filter = ctx->get_argument_data_from_descriptor<central_repo_key_filter>(0);
        std::uint32_t *found_uid_result_array = reinterpret_cast<std::uint32_t *>(ctx->get_descriptor_argument_ptr(2));
        const std::size_t found_uid_max_uids = (ctx->get_argument_max_data_size(2) / sizeof(std::uint32_t)) - 1;

        if (!filter || !found_uid_result_array) {
            LOG_ERROR(SERVICE_CENREP, "Trying to find equal value in cenrep, but arguments are invalid!");
            ctx->complete(epoc::error_argument);
            return;
        }

        found_uid_result_array[0] = 0;
        std::string cache_arg;

        for (auto &entry : attach_repo->entries) {
            if ((entry.key & filter->id_mask) != (filter->partial_key & filter->id_mask)) {
                continue;
            }

            std::uint32_t key_found = 0;
            bool find_not_eq = false;

            switch (ctx->msg->function) {
            case cen_rep_find_neq_int:
            case cen_rep_find_neq_real:
            case cen_rep_find_neq_string:
                find_not_eq = true;
                break;

            default:
                break;
            }

            switch (ctx->msg->function) {
            case cen_rep_find_eq_int:
            case cen_rep_find_neq_int: {
                if (entry.data.etype != central_repo_entry_type::integer) {
                    break;
                }

                if (static_cast<std::int32_t>(entry.data.intd) == *ctx->get_argument_value<std::int32_t>(1)) {
                    if (!find_not_eq) {
                        key_found = entry.key;
                    }
                } else {
                    if (find_not_eq) {
                        key_found = entry.key;
                    }
                }

                break;
            }

            case cen_rep_find_eq_string:
            case cen_rep_find_neq_string: {
                bool is_ok = false;

                if (cache_arg.empty()) {
                    auto ss = ctx->get_argument_value<std::string>(1);

                    if (!ss.has_value()) {
                        ctx->complete(epoc::error_argument);
                        return;
                    }

                    cache_arg = ss.value();
                }

                if (entry.data.etype == central_repo_entry_type::string) {
                    is_ok = (entry.data.strd == cache_arg);
                } else {
                    break;
                }

                if (is_ok) {
                    if (!find_not_eq) {
                        key_found = entry.key;
                    }
                } else {
                    if (find_not_eq) {
                        key_found = entry.key;
                    }
                }
                break;
            }

            case cen_rep_find:
                key_found = entry.key;
                break;

            default: {
                LOG_ERROR(SERVICE_CENREP, "Unimplemented Cenrep's find function for opcode {}", ctx->msg->function);
                break;
            }
            }

            if (key_found != 0) {
                append_new_key_to_found_eq_list(found_uid_result_array, key_found, static_cast<std::uint32_t>(found_uid_max_uids));
            }
        }

        if (found_uid_result_array[0] == 0) {
            ctx->complete(epoc::error_not_found);
            return;
        }

        ctx->complete(epoc::error_none);
    }

    void central_repo_client_subsession::get_find_result(service::ipc_context *ctx) {
        std::uint32_t *found_uid_result_array = reinterpret_cast<std::uint32_t *>(ctx->get_descriptor_argument_ptr(0));
        const std::size_t found_uid_max_uids = (ctx->get_argument_max_data_size(0) / sizeof(std::uint32_t));

        ctx->write_data_to_descriptor_argument(0, reinterpret_cast<std::uint8_t *>(&key_found_result[0]), static_cast<std::uint32_t>(common::min(found_uid_max_uids, key_found_result.size()) * sizeof(std::uint32_t)));

        ctx->complete(epoc::error_none);
    }

    void central_repo_client_subsession::notify_nof_check(service::ipc_context *ctx) {
        epoc::notify_info holder;
        holder.sts = 0;

        if (add_notify_request(holder, 0xFFFFFFFF, *ctx->get_argument_value<std::int32_t>(0)) == 0) {
            ctx->complete(epoc::error_none);
            return;
        }

        ctx->complete(epoc::error_already_exists);
    }

    void central_repo_client_subsession::notify(service::ipc_context *ctx) {
        const std::uint32_t mask = (ctx->msg->function == cen_rep_notify_req) ? 0xFFFFFFFF : *ctx->get_argument_value<std::uint32_t>(1);
        const std::uint32_t partial_key = *ctx->get_argument_value<std::uint32_t>(0);

        epoc::notify_info info{ ctx->msg->request_sts, ctx->msg->own_thr };
        const int err = add_notify_request(info, mask, partial_key);

        switch (err) {
        case 0: {
            break;
        }

        case -1: {
            ctx->complete(epoc::error_already_exists);
            break;
        }

        default: {
            LOG_TRACE(SERVICE_CENREP, "Unknown returns code {} from add_notify_request, set status to epoc::error_none", err);
            ctx->complete(epoc::error_none);

            break;
        }
        }
    }

    void central_repo_client_subsession::notify_cancel(service::ipc_context *ctx) {
        std::uint32_t mask = 0;
        std::uint32_t partial_key = 0;

        if (ctx->msg->function == cen_rep_notify_cancel) {
            mask = 0xFFFFFFFF;
            partial_key = *ctx->get_argument_value<std::uint32_t>(0);
        } else {
            std::optional<central_repo_key_filter> filter = ctx->get_argument_data_from_descriptor<central_repo_key_filter>(0);

            if (!filter.has_value()) {
                ctx->complete(epoc::error_argument);
                return;
            }

            mask = filter->id_mask;
            partial_key = filter->partial_key;
        }

        cancel_notify_request(partial_key, mask);
        ctx->complete(epoc::error_none);
    }

    void central_repo_client_subsession::start_transaction(service::ipc_context *ctx) {
        LOG_TRACE(SERVICE_CENREP, "TransactionStart stubbed");
        ctx->complete(epoc::error_none);
    }

    void central_repo_client_subsession::cancel_transaction(service::ipc_context *ctx) {
        LOG_TRACE(SERVICE_CENREP, "TransactionCancel stubbed");
        ctx->complete(epoc::error_none);
    }

    void central_repo_client_subsession::handle_message(service::ipc_context *ctx) {
        switch (ctx->msg->function) {
        case cen_rep_create_int:
        case cen_rep_create_real:
        case cen_rep_create_string:
            create_value(ctx);
            break;

        case cen_rep_notify_req_check:
            notify_nof_check(ctx);
            break;

        case cen_rep_group_nof_cancel:
        case cen_rep_notify_cancel:
            notify_cancel(ctx);
            break;

        case cen_rep_notify_cancel_all:
            cancel_all_notify_requests();
            ctx->complete(epoc::error_none);
            break;

        case cen_rep_group_nof_req:
        case cen_rep_notify_req:
            notify(ctx);
            break;

        case cen_rep_get_int:
        case cen_rep_get_real:
        case cen_rep_get_string:
            get_value(ctx);
            break;

        case cen_rep_set_int:
        case cen_rep_set_string:
        case cen_rep_set_real:
            set_value(ctx);
            break;

        case cen_rep_reset:
            reset(ctx);
            break;

        case cen_rep_find_eq_int:
        case cen_rep_find_eq_real:
        case cen_rep_find_eq_string:
        case cen_rep_find_neq_int:
        case cen_rep_find_neq_real:
        case cen_rep_find_neq_string:
        case cen_rep_find: {
            find(ctx);
            break;
        }

        case cen_rep_transaction_start:
            start_transaction(ctx);
            break;

        case cen_rep_transaction_cancel:
            cancel_transaction(ctx);
            break;

        case cen_rep_get_find_res:
            get_find_result(ctx);
            break;

        default: {
            LOG_ERROR(SERVICE_CENREP, "Unhandled message opcode for cenrep 0x{:X}", ctx->msg->function);
            break;
        }
        }
    }
}