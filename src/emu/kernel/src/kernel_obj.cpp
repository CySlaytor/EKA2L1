#include <kernel/kernel.h>
#include <kernel/kernel_obj.h>

#include <common/chunkyseri.h>
#include <common/log.h>

#include <cstdio>

namespace eka2l1 {
    namespace kernel {
        kernel_obj::kernel_obj(kernel_system *kern, const std::string &obj_name, kernel_obj *owner, kernel::access_type access)
            : obj_name(obj_name)
            , owner(owner)
            , kern(kern)
            , access(access)
            , uid(kern->next_uid()) {

            if (!this->obj_name.empty() && this->obj_name.back() == '\0') {
                this->obj_name.pop_back();
            }

            if (owner) {
                owner->increase_access_count();
            }
        }

        void kernel_obj::do_state(common::chunkyseri &seri) {
            auto s = seri.section("KernelObject", 1);

            if (!s) {
                return;
            }

            seri.absorb(obj_name);
            seri.absorb(uid);
            seri.absorb(obj_type);
            seri.absorb(access);
            seri.absorb(access_count);
        }

        void kernel_obj::full_name(std::string &name_will_full) {
            if (owner && (access != kernel::access_type::global_access)) {
                owner->full_name(name_will_full);
                name_will_full += "::";
            }

            name_will_full += name();
        }

        void kernel_obj::increase_access_count() {
            ++access_count;
        }

        int kernel_obj::decrease_access_count() {
            if (!kern->wipeout_in_progress()) {
                if (--access_count == 0) {
                    return kern->destroy(this);
                }
            }

            return 0;
        }
    }
}