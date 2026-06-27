#include <services/centralrepo/centralrepo.h>
#include <services/centralrepo/repo.h>
#include <services/ui/skin/common.h>
#include <services/ui/skin/icon_cfg.h>

namespace eka2l1::epoc {
    akn_skin_icon_config_map::akn_skin_icon_config_map(central_repo_server *cenrep_,
        device_manager *mngr, io_system *io_, const language lang_)
        : cenrep_serv_(cenrep_)
        , io_(io_)
        , mngr_(mngr)
        , inited_(false)
        , sys_lang_(lang_) {
    }

    void akn_skin_icon_config_map::read_and_parse_repo() {
        eka2l1::central_repo *repo = cenrep_serv_->load_repo_with_lookup(io_, mngr_, ICON_CAPTION_UID);
        if (!repo)
            return;

        std::vector<eka2l1::central_repo_entry *> app_uids_entries;
        repo->query_entries(0x00FFFFFF, 0x00FFFFFF, app_uids_entries, central_repo_entry_type::integer);

        for (auto &app_uid_entry : app_uids_entries) {
            const std::uint32_t app_uid_key = static_cast<int>(app_uid_entry->data.intd);
            central_repo_entry *ent = repo->find_entry(app_uid_key);
            if (!ent || ent->data.etype != central_repo_entry_type::integer)
                return;

            const epoc::uid app_uid_val = static_cast<epoc::uid>(ent->data.intd);

            std::vector<eka2l1::central_repo_entry *> app_key_entries;
            repo->query_entries(app_uid_key, 0xFF000000, app_key_entries, central_repo_entry_type::integer);

            for (auto &app_key_entry : app_key_entries) {
                const std::uint32_t app_config_key = static_cast<std::uint32_t>(app_key_entry->data.intd);

                if (app_config_key >> 16 == 2) {
                    language app_lang = static_cast<language>(app_config_key & 0x0000FFFF);
                    ent = repo->find_entry(app_config_key);
                    if (!ent || ent->data.etype != central_repo_entry_type::integer)
                        return;
                    if (static_cast<int>(ent->data.intd) > 0 && app_lang == sys_lang_) {
                        cfgs_.push_back(app_uid_val);
                    }
                }
            }
        }
        std::sort(cfgs_.begin(), cfgs_.end());
    }

    int akn_skin_icon_config_map::is_icon_configured(const epoc::uid app_uid) {
        if (!inited_) {
            read_and_parse_repo();
            inited_ = true;
        }
        if (cfgs_.empty())
            return -1;
        if (std::lower_bound(cfgs_.begin(), cfgs_.end(), app_uid) != cfgs_.end())
            return 1;
        return 0;
    }
}