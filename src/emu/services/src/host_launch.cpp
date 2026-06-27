#include <services/applist/applist.h>
#include <services/host_launch.h>

#include <kernel/kernel.h>
#include <system/epoc.h>

#include <common/applauncher.h>
#include <common/log.h>
#include <utils/apacmd.h>

namespace eka2l1::service {
    void init_symbian_app_launch_to_host_launch(system *sys) {
        kernel_system *kern = sys->get_kernel_system();

        applist_server *serv = kern->get_by_name<applist_server>(
            get_app_list_server_name_by_epocver(kern->get_epoc_version()));

        if (!serv) {
            LOG_ERROR(SERVICE_TRACK, "Unable to initialize Symbian app launch to host app launch: Applist server is not available!");
            return;
        }

        kern->register_guomen_process_run_callback([](kernel::process *pr) {
            std::optional<kernel::pass_arg> launch_arg_raw = pr->get_arg_slot(epoc::apa::PROCESS_ENVIRONMENT_ARG_SLOT_MAIN);
            if (!launch_arg_raw.has_value()) {
                LOG_ERROR(SERVICE_APPLIST, "Unable to perform launching host app instead of Symbian app: environment variable argument not available!");
                return false;
            }

            common::chunkyseri seri(launch_arg_raw->data.data(), launch_arg_raw->data.size(),
                common::SERI_MODE_READ);

            epoc::apa::command_line launch_cmdline;
            launch_cmdline.do_it_newarch(seri);

            std::u16string run_command = launch_cmdline.executable_path_;

            if (run_command.find(MAPPED_EXECUTABLE_HEAD_STRING) == 0) {
                run_command = run_command.substr(MAPPED_EXECUTABLE_HEAD_STRING_LENGTH + 1,
                    run_command.length() - MAPPED_EXECUTABLE_HEAD_STRING_LENGTH - 1 - UNIQUE_MAPPED_EXTENSION_STRING_LENGTH);
            }

            LOG_ERROR(SERVICE_APPLIST, "No host launcher correspond for launch command: {}", common::ucs2_to_utf8(run_command));
            return false;
        });
    }
}