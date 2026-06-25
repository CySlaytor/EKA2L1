#include <common/uid.h>
#include <utils/chunk.h>
#include <utils/des.h>
#include <utils/dll.h>
#include <utils/event.h>
#include <utils/handle.h>
#include <utils/panic.h>
#include <utils/reqsts.h>
#include <utils/uchar.h>

#include <common/common.h>
#include <common/configure.h>
#include <kernel/kernel.h>
#include <kernel/svc.h>

#include <loader/rom.h>

#include <config/config.h>

#include <common/algorithm.h>
#include <common/cvt.h>
#include <common/path.h>
#include <common/platform.h>
#include <common/random.h>
#include <common/time.h>
#include <common/types.h>
#include <utils/locale.h>
#include <utils/system.h>

#include <ctime>
#include <utils/err.h>

namespace eka2l1::epoc {
    static security_policy server_exclamation_point_name_policy({ cap_prot_serv });
    static security_policy kill_process_policy({ cap_power_mgmt });

    static eka2l1::kernel::thread_local_data *current_local_data(kernel_system *kern) {
        return kern->crr_thread()->get_local_data();
    }

    static codeseg_ptr get_codeseg_from_addr(kernel_system *kern, kernel::process *pr, const std::uint32_t addr,
        const bool ep) {
        hle::lib_manager &mngr = *kern->get_lib_manager();

        for (const auto &seg_obj : kern->get_codeseg_list()) {
            codeseg_ptr seg = reinterpret_cast<codeseg_ptr>(seg_obj.get());

            bool cond2 = (ep && (seg->get_entry_point(pr) == addr));
            if (!cond2) {
                const address beg = seg->get_code_run_addr(pr);
                const address end = seg->get_text_size() + beg;

                if ((beg <= addr) && (addr <= end)) {
                    cond2 = true;
                }
            }

            if (seg && cond2) {
                return seg;
            }
        }

        return nullptr;
    }

    static std::optional<std::u16string> get_dll_full_path(kernel_system *kern, const std::uint32_t addr) {
        codeseg_ptr ss = get_codeseg_from_addr(kern, kern->crr_process(), addr, true);
        if (ss) {
            return ss->get_full_path();
        }

        return std::nullopt;
    }

    static address get_exception_descriptor_addr(kernel_system *kern, address runtime_addr) {
        hle::lib_manager *manager = kern->get_lib_manager();
        kernel::process *crr_process = kern->crr_process();

        for (const auto &seg_obj : kern->get_codeseg_list()) {
            codeseg_ptr seg = reinterpret_cast<codeseg_ptr>(seg_obj.get());
            const address seg_code_addr = seg->get_code_run_addr(crr_process);

            if (seg_code_addr <= runtime_addr && seg_code_addr + seg->get_code_size() >= runtime_addr) {
                return seg->get_exception_descriptor(crr_process);
            }
        }

        return 0;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, heap) {
        kernel::thread_local_data *local_data = current_local_data(kern);

        if (local_data->heap.ptr_address() == 0) {
            LOG_WARN(KERNEL, "Allocator is not available.");
        }

        return local_data->heap;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, heap_switch, eka2l1::ptr<void> new_heap) {
        kernel::thread_local_data *local_data = current_local_data(kern);
        eka2l1::ptr<void> old_heap = local_data->heap;
        local_data->heap = new_heap;

        return old_heap;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, trap_handler) {
        kernel::thread_local_data *local_data = current_local_data(kern);
        return local_data->trap_handler;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, set_trap_handler, eka2l1::ptr<void> new_handler) {
        kernel::thread_local_data *local_data = current_local_data(kern);
        eka2l1::ptr<void> old_handler = local_data->trap_handler;
        local_data->trap_handler = new_handler;

        return old_handler;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, active_scheduler) {
        kernel::thread_local_data *local_data = current_local_data(kern);
        return local_data->scheduler;
    }

    BRIDGE_FUNC(void, set_active_scheduler, eka2l1::ptr<void> new_scheduler) {
        kernel::thread_local_data *local_data = current_local_data(kern);
        local_data->scheduler = new_scheduler;
    }

    BRIDGE_FUNC(void, after, std::int32_t micro_secs, eka2l1::ptr<epoc::request_status> status) {
        kernel::thread *thr = kern->crr_thread();
        thr->sleep_nof(status, micro_secs);
    }

    BRIDGE_FUNC(std::int32_t, process_exit_type, kernel::handle h) {
        process_ptr pr_real = kern->get<kernel::process>(h);

        if (!pr_real) {
            LOG_ERROR(KERNEL, "process_exit_type: Invalid process");
            return 0;
        }

        return static_cast<std::int32_t>(pr_real->get_exit_type());
    }

    BRIDGE_FUNC(std::int32_t, process_exit_reason, kernel::handle h) {
        process_ptr pr_real = kern->get<kernel::process>(h);

        if (!pr_real) {
            LOG_ERROR(KERNEL, "process_exit_reason: Invalid process");
            return 0;
        }

        return static_cast<std::int32_t>(pr_real->get_exit_reason());
    }

    BRIDGE_FUNC(void, process_exit_category, kernel::handle h, epoc::des8 *category) {
        process_ptr pr_real = kern->get<kernel::process>(h);
        process_ptr cr_process = kern->crr_process();

        if (!pr_real) {
            LOG_ERROR(KERNEL, "Invalid handle passed to get process's exit category");
            return;
        }

        if (!category) {
            LOG_ERROR(KERNEL, "Invalid descriptor pointer passed to get process's exit category");
            return;
        }

        category->assign(cr_process, common::ucs2_to_utf8(cr_process->get_exit_category()));
    }

    BRIDGE_FUNC(void, process_rendezvous, std::int32_t complete_code) {
        kernel::process *pr = kern->crr_process();
        pr->rendezvous(complete_code);
    }

    BRIDGE_FUNC(void, process_filename, std::int32_t process_handle, eka2l1::ptr<des8> name) {
        process_ptr pr_real = kern->get<kernel::process>(process_handle);
        process_ptr cr_process = kern->crr_process();

        if (!pr_real) {
            LOG_ERROR(KERNEL, "Svcprocess_filename: Invalid process");
            return;
        }

        epoc::des8 *des = name.get(cr_process);
        const std::u16string full_path_u16 = pr_real->get_exe_path();

        const std::string full_path = common::ucs2_to_utf8(full_path_u16);
        des->assign(cr_process, full_path);
    }

    static void get_memory_info_from_codeseg(codeseg_ptr seg, kernel::process *pr, kernel::memory_info *info) {
        info->rt_code_addr = seg->get_code_run_addr(pr);
        info->rt_code_size = (seg->is_rom() ? 0 : seg->get_text_size());

        info->rt_const_data_addr = info->rt_code_addr + seg->get_text_size();
        info->rt_const_data_size = (seg->is_rom() ? 0 : (seg->get_code_size() - seg->get_text_size()));

        info->rt_initialized_data_addr = seg->get_data_run_addr(pr);
        info->rt_initialized_data_size = seg->get_data_size();

        info->rt_bss_addr = info->rt_initialized_data_addr + info->rt_initialized_data_size;
        info->rt_bss_size = seg->get_bss_size();
    }

    BRIDGE_FUNC(std::int32_t, process_get_memory_info, kernel::handle h, eka2l1::ptr<kernel::memory_info> info) {
        kernel::memory_info *info_host = info.get(kern->crr_process());
        process_ptr pr_real = kern->get<kernel::process>(h);

        if (!pr_real) {
            return epoc::error_bad_handle;
        }

        get_memory_info_from_codeseg(pr_real->get_codeseg(), pr_real, info_host);
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, process_get_id, kernel::handle h) {
        process_ptr pr_real = kern->get<kernel::process>(h);

        if (!pr_real) {
            LOG_ERROR(KERNEL, "process_get_id: Invalid process");
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(pr_real->unique_id());
    }

    BRIDGE_FUNC(void, process_type, kernel::handle h, eka2l1::ptr<epoc::uid_type> uid_type) {
        process_ptr pr_real = kern->get<kernel::process>(h);

        if (!pr_real) {
            LOG_ERROR(KERNEL, "Svcprocess_type: Invalid process");
            return;
        }

        process_ptr crr_pr = kern->crr_process();

        epoc::uid_type *type = uid_type.get(crr_pr);
        auto tup = pr_real->get_uid_type();

        type->uid1 = std::get<0>(tup);
        type->uid2 = std::get<1>(tup);
        type->uid3 = std::get<2>(tup);
    }

    BRIDGE_FUNC(std::int32_t, process_data_parameter_length, std::int32_t slot) {
        kernel::process *crr_process = kern->crr_process();

        if (slot >= 16 || slot < 0) {
            LOG_ERROR(KERNEL, "Invalid slot (slot: {} >= 16 or < 0)", slot);
            return epoc::error_argument;
        }

        auto slot_ptr = crr_process->get_arg_slot(slot);

        if (!slot_ptr || !slot_ptr->used) {
            LOG_ERROR(KERNEL, "Getting descriptor length of unused slot: {}", slot);
            return epoc::error_not_found;
        }

        return static_cast<std::int32_t>(slot_ptr->data.size());
    }

    BRIDGE_FUNC(std::int32_t, process_set_handle_parameter, kernel::handle process, std::int32_t slot_num, kernel::handle target) {
        kernel::process *pr = kern->get<kernel::process>(process);
        if (!pr) {
            return epoc::error_bad_handle;
        }

        if (slot_num >= 16 || slot_num < 0) {
            LOG_ERROR(KERNEL, "Invalid slot (slot: {} >= 16 or < 0)", slot_num);
            return epoc::error_argument;
        }

        const bool result = pr->set_arg_slot(static_cast<std::uint8_t>(slot_num), reinterpret_cast<std::uint8_t *>(&target), 4, true);

        if (!result) {
            LOG_ERROR(KERNEL, "Parameter slot used, slot number: {}", slot_num);
            return epoc::error_in_use;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(kernel::handle, process_get_handle_parameter, std::int32_t slot_num, kernel::object_type obj_type, kernel::owner_type own) {
        kernel::process *pr = kern->crr_process();

        if (slot_num >= 16 || slot_num < 0) {
            LOG_ERROR(KERNEL, "Invalid slot (slot: {} >= 16 or < 0)", slot_num);
            return epoc::error_argument;
        }

        std::optional<kernel::handle> result = pr->get_handle_from_arg_slot(static_cast<std::uint8_t>(slot_num), obj_type, own);

        if (!result.has_value()) {
            return epoc::error_not_found;
        }

        return result.value();
    }

    BRIDGE_FUNC(std::int32_t, process_get_data_parameter, std::int32_t slot_num, eka2l1::ptr<std::uint8_t> data_ptr, std::int32_t length) {
        kernel::process *pr = kern->crr_process();

        if (slot_num >= 16 || slot_num < 0) {
            LOG_ERROR(KERNEL, "Invalid slot (slot: {} >= 16 or < 0)", slot_num);
            return epoc::error_argument;
        }

        auto slot = *pr->get_arg_slot(slot_num);

        if (!slot.used) {
            LOG_ERROR(KERNEL, "Parameter slot unused, error: {}", slot_num);
            return epoc::error_not_found;
        }

        if (length < slot.data.size()) {
            LOG_ERROR(KERNEL, "Given length is not large enough to slot length ({} vs {})",
                length, slot.data.size());
            return epoc::error_no_memory;
        }

        std::uint8_t *data = data_ptr.get(pr);
        std::copy(slot.data.begin(), slot.data.end(), data);

        std::size_t written_size = slot.data.size();
        pr->mark_slot_free(static_cast<std::uint8_t>(slot_num));

        return static_cast<std::int32_t>(written_size);
    }

    BRIDGE_FUNC(std::int32_t, process_set_data_parameter, kernel::handle h, std::int32_t slot_num, eka2l1::ptr<std::uint8_t> data, std::int32_t data_size) {
        process_ptr pr = kern->get<kernel::process>(h);

        if (!pr) {
            return epoc::error_bad_handle;
        }

        if (slot_num < 0 || slot_num >= 16) {
            LOG_ERROR(KERNEL, "Invalid parameter slot: {}, slot number must be in range of 0-15", slot_num);
            return epoc::error_argument;
        }

        auto slot = *pr->get_arg_slot(slot_num);

        if (slot.used) {
            LOG_ERROR(KERNEL, "Can't set parameter of an used slot: {}", slot_num);
            return epoc::error_in_use;
        }

        pr->set_arg_slot(slot_num, data.get(kern->crr_process()), data_size);
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, process_command_line_length, kernel::handle h) {
        process_ptr pr = kern->get<kernel::process>(h);

        if (!pr) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(pr->get_cmd_args().length());
    }

    BRIDGE_FUNC(void, process_command_line, kernel::handle h, eka2l1::ptr<epoc::des8> data_des) {
        process_ptr pr = kern->get<kernel::process>(h);

        if (!pr) {
            LOG_WARN(KERNEL, "Process not found with handle: 0x{:x}", h);
            return;
        }

        kernel::process *crr_process = kern->crr_process();
        epoc::des8 *data = data_des.get(crr_process);

        if (!data)
            return;

        std::u16string cmdline = pr->get_cmd_args();


        char *data_ptr = data->get_pointer(crr_process);
        memcpy(data_ptr, cmdline.data(), cmdline.length() << 1);
        data->set_length(crr_process, static_cast<std::uint32_t>(cmdline.length() << 1));
    }

    BRIDGE_FUNC(void, process_set_flags, kernel::handle h, std::uint32_t clear_mask, std::uint32_t set_mask) {
        process_ptr pr = kern->get<kernel::process>(h);

        uint32_t org_flags = pr->get_flags();
        uint32_t new_flags = ((org_flags & ~clear_mask) | set_mask);
        new_flags = (new_flags ^ org_flags);

        pr->set_flags(org_flags ^ new_flags);
    }

    BRIDGE_FUNC(std::int32_t, process_set_priority, kernel::handle h, std::int32_t process_priority) {
        process_ptr pr = kern->get<kernel::process>(h);

        if (!pr) {
            return epoc::error_bad_handle;
        }

        pr->set_priority(static_cast<eka2l1::kernel::process_priority>(process_priority));
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, process_priority, kernel::handle h) {
        process_ptr pr = kern->get<kernel::process>(h);

        if (!pr) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(pr->get_priority());
    }

    BRIDGE_FUNC(std::int32_t, process_id, kernel::handle h) {
        process_ptr pr = kern->get<kernel::process>(h);

        if (!pr) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(pr->unique_id());
    }

    BRIDGE_FUNC(std::int32_t, process_rename, kernel::handle h, eka2l1::ptr<des8> new_name) {
        process_ptr pr = kern->get<kernel::process>(h);
        process_ptr cur_pr = kern->crr_process();

        if (!pr) {
            return epoc::error_bad_handle;
        }

        const std::string new_name_str = new_name.get(cur_pr)->to_std_string(cur_pr);
        pr->rename(new_name_str);

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, process_resume, kernel::handle h) {
        process_ptr pr = kern->get<kernel::process>(h);

        if (!pr) {
            return;
        }

        pr->run();
    }

    BRIDGE_FUNC(void, process_logon, kernel::handle h, eka2l1::ptr<epoc::request_status> req_sts, bool rendezvous) {
        process_ptr pr = kern->get<kernel::process>(h);

        if (!pr) {
            return;
        }

        pr->logon(req_sts, rendezvous);
    }

    BRIDGE_FUNC(std::int32_t, process_logon_cancel, kernel::handle h, eka2l1::ptr<epoc::request_status> request_sts, bool rendezvous) {
        process_ptr pr = kern->get<kernel::process>(h);

        if (!pr) {
            return epoc::error_bad_handle;
        }

        bool logon_cancel_success = pr->logon_cancel(request_sts, rendezvous);

        if (logon_cancel_success) {
            return epoc::error_none;
        }

        return epoc::error_general;
    }

    BRIDGE_FUNC(void, process_kill, kernel::handle h, kernel::entity_exit_type etype, std::int32_t reason, epoc::desc16 *category) {
        process_ptr pr = kern->get<kernel::process>(h);

        if (!pr) {
            return;
        }

        if (!kern->is_eka1() && pr != kern->crr_process()) {
            if (!pr->satisfy(kill_process_policy)) {
                LOG_ERROR(KERNEL, "Process kill failed, process {} doesn't have enough capability to kill process {}",
                    kern->crr_process()->name(), pr->name());

                return;
            }
        }

        pr->kill(etype, category ? category->to_std_string(kern->crr_process()) : u"None", reason);
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, dll_tls, kernel::handle h, std::int32_t dll_uid) {
        kernel::thread *thr = kern->crr_thread();
        std::optional<kernel::tls_slot> slot = thr->get_tls_slot(h, dll_uid);

        if (slot.has_value()) {
            return slot->pointer;
        }

        return eka2l1::ptr<void>(0);
    }

    BRIDGE_FUNC(std::int32_t, dll_set_tls, kernel::handle h, std::int32_t dll_uid, eka2l1::ptr<void> data_set) {
        kernel::thread *thr = kern->crr_thread();

        if (!thr->set_tls_slot(h, dll_uid, data_set)) {
            return epoc::error_no_memory;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, dll_free_tls, kernel::handle h) {
        kernel::thread *thr = kern->crr_thread();
        thr->close_tls_slot(h);
    }

    BRIDGE_FUNC(void, dll_filename, std::int32_t entry_addr, eka2l1::ptr<epoc::des8> full_path_ptr) {
        std::optional<std::u16string> dll_full_path = get_dll_full_path(kern, entry_addr);

        if (!dll_full_path) {
            LOG_WARN(KERNEL, "Unable to find DLL name for address: 0x{:x}", entry_addr);
            return;
        }

        std::string path_utf8 = common::ucs2_to_utf8(*dll_full_path);
        LOG_TRACE(KERNEL, "Find DLL for address 0x{:x} with name: {}", static_cast<std::uint32_t>(entry_addr),
            path_utf8);

        kernel::process *crr_pr = kern->crr_process();
        full_path_ptr.get(crr_pr)->assign(crr_pr, path_utf8);
    }

    BRIDGE_FUNC(std::int32_t, utc_offset) {
        return kern->utc_offset();
    }

    BRIDGE_FUNC(std::int32_t, time_now, eka2l1::ptr<std::uint64_t> time_ptr, eka2l1::ptr<std::int32_t> utc_offset_ptr) {
        kernel::process *pr = kern->crr_process();

        std::uint64_t *time = time_ptr.get(pr);
        std::int32_t *offset = utc_offset_ptr.get(kern->crr_process());

        *time = kern->universal_time();
        *offset = kern->utc_offset();

        if (kern->is_eka1()) {
            if (pr->get_time_delay())
                kern->crr_thread()->sleep(pr->get_time_delay());
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, set_utc_time_and_offset, eka2l1::ptr<std::uint64_t> time_ptr, std::int32_t utc_offset,
        std::uint32_t mode, std::uint32_t changes) {
        std::uint64_t *time = time_ptr.get(kern->crr_process());

        if (mode & time_set_time)
            kern->set_base_time(*time - kern->get_ntimer()->microseconds());

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, message_construct, std::int32_t msg_handle, service::message2 *msg_to_construct) {
        ipc_msg_ptr msg = kern->get_msg(msg_handle);

        if (!msg) {
            return;
        }

        msg_to_construct->ipc_msg_handle = msg_handle;
        msg_to_construct->session_ptr = msg->session_ptr_lle;
        msg_to_construct->flags = msg->args.flag;
        msg_to_construct->function = msg->function;
        std::copy(msg->args.args, msg->args.args + 4, msg_to_construct->args);
    }

    BRIDGE_FUNC(void, set_session_ptr, std::int32_t msg_handle, std::uint32_t session_addr) {
        ipc_msg_ptr msg = kern->get_msg(msg_handle);

        if (!msg) {
            return;
        }

        msg->msg_session->set_cookie_address(session_addr);
    }

    BRIDGE_FUNC(void, message_complete, std::int32_t msg_handle, std::int32_t val) {
        ipc_msg_ptr msg = kern->get_msg(msg_handle);

        if (msg->request_sts) {
            epoc::request_status *status = msg->request_sts.get(msg->own_thr->owning_process());

            if (status)
                status->set(val, kern->is_eka1());

            msg->own_thr->signal_request();
            if (kern->get_config()->log_ipc)
                LOG_TRACE(KERNEL, "Message completed with code: {}, thread to signal: {}", val, msg->own_thr->name());
        }

        if (msg->thread_handle_low) {
            kern->close(msg->thread_handle_low);
        }

        kern->call_ipc_complete_callbacks(msg, val);
        msg->unref();
    }

    BRIDGE_FUNC(void, message_complete_handle, std::int32_t msg_handle, std::int32_t handle) {
        ipc_msg_ptr msg = kern->get_msg(msg_handle);
        std::uint32_t dup_handle = kern->mirror(msg->own_thr, handle, kernel::owner_type::thread);

        if (dup_handle == kernel::INVALID_HANDLE) {
            LOG_ERROR(KERNEL, "Fail to duplicate handle to client thread for message completion!");
        }

        if (msg->request_sts) {
            (msg->request_sts.get(msg->own_thr->owning_process()))->set(static_cast<std::int32_t>(dup_handle), kern->is_eka1());
            msg->own_thr->signal_request();
        }

        if (kern->get_config()->log_ipc)
            LOG_TRACE(KERNEL, "Message completed with handle: {}, thread to signal: {}", dup_handle, msg->own_thr->name());

        kern->call_ipc_complete_callbacks(msg, dup_handle);
        msg->unref();
    }

    BRIDGE_FUNC(void, message_kill, kernel::handle h, kernel::entity_exit_type etype, std::int32_t reason, eka2l1::ptr<desc8> cage) {
        process_ptr crr = kern->crr_process();

        std::string exit_cage = cage.get(crr)->to_std_string(kern->crr_process());
        std::optional<std::string> exit_description;

        ipc_msg_ptr msg = kern->get_msg(h);

        std::string exit_category = "None";
        kernel::process *pr = kern->crr_process();

        if (cage) {
            exit_category = cage.get(pr)->to_std_string(pr);
        }

        msg->own_thr->kill(etype, common::utf8_to_ucs2(exit_category), reason);
    }

    BRIDGE_FUNC(std::int32_t, message_get_des_length, kernel::handle h, std::int32_t param) {
        if (param < 0) {
            return epoc::error_argument;
        }

        ipc_msg_ptr msg = kern->get_msg(h);

        if (!msg) {
            return epoc::error_bad_handle;
        }

        if ((int)msg->args.get_arg_type(param) & (int)ipc_arg_type::flag_des) {
            epoc::desc_base *base = eka2l1::ptr<epoc::desc_base>(msg->args.args[param]).get(msg->own_thr->owning_process());

            return base->get_length();
        }

        return epoc::error_bad_descriptor;
    }

    BRIDGE_FUNC(std::int32_t, message_get_des_max_length, kernel::handle h, std::int32_t param) {
        if (param < 0) {
            return epoc::error_argument;
        }

        ipc_msg_ptr msg = kern->get_msg(h);

        if (!msg) {
            return epoc::error_bad_handle;
        }

        const ipc_arg_type type = msg->args.get_arg_type(param);

        if ((int)type & (int)ipc_arg_type::flag_des) {
            kernel::process *own_pr = msg->own_thr->owning_process();
            return eka2l1::ptr<epoc::des8>(msg->args.args[param]).get(own_pr)->get_max_length(own_pr);
        }

        return epoc::error_bad_descriptor;
    }

    std::int32_t do_ipc_manipulation(kernel_system *kern, kernel::thread *client_thread, std::uint8_t *client_ptr, ipc_copy_info &copy_info,
        std::int32_t start_offset) {
        bool des8 = true;
        if (copy_info.flags & CHUNK_SHIFT_BY_1) {
            des8 = false;
        }

        bool read = true;
        if (copy_info.flags & IPC_DIR_WRITE) {
            read = false;
        }

        process_ptr callee_process = client_thread->owning_process();

        epoc::des8 *the_des = reinterpret_cast<epoc::des8 *>(client_ptr);

        if (!the_des->is_valid_descriptor()) {
            return epoc::error_bad_descriptor;
        }

        std::int32_t size_of_work = read ? (the_des->get_length() - start_offset) : (the_des->get_max_length(callee_process) - start_offset);
        client_ptr = reinterpret_cast<std::uint8_t *>(the_des->get_pointer_raw(callee_process));

        if (read && (size_of_work > copy_info.target_length)) {
            size_of_work = copy_info.target_length;
        }

        if (!read && (size_of_work < copy_info.target_length)) {
            return epoc::error_overflow;
        }

        process_ptr crr_process = kern->crr_process();
        std::uint8_t *info_host_ptr = (copy_info.flags & IPC_HLE_EKA1) ? copy_info.target_host_ptr : copy_info.target_ptr.get(crr_process);

        size_of_work = common::min<std::uint32_t>(size_of_work, copy_info.target_length);

        if ((size_of_work > 0) && !info_host_ptr) {
            return epoc::error_argument;
        }

        if (!read && the_des) {
            the_des->set_length(callee_process, size_of_work + start_offset);
        }

        std::size_t raw_size_of_work = size_of_work;

        if (!des8) {
            raw_size_of_work *= sizeof(std::uint16_t);
            start_offset *= sizeof(std::uint16_t);
        }

        if (size_of_work > 0) {
            std::memcpy(read ? info_host_ptr : (client_ptr + start_offset), read ? (client_ptr + start_offset) : info_host_ptr, raw_size_of_work);
        }
        return (read ? static_cast<std::int32_t>(size_of_work) : epoc::error_none);
    }

    BRIDGE_FUNC(std::int32_t, message_ipc_copy, kernel::handle h, std::int32_t param, eka2l1::ptr<ipc_copy_info> info,
        std::int32_t start_offset) {
        if (!info || param < 0 || param > 3) {
            return epoc::error_argument;
        }

        process_ptr crr_process = kern->crr_process();

        ipc_copy_info *info_host = info.get(crr_process);
        ipc_msg_ptr msg = kern->get_msg(h);

        if (!msg) {
            return epoc::error_bad_handle;
        }

        msg->ref();

        ipc_arg_type arg_type = msg->args.get_arg_type(param);
        if (!(static_cast<std::uint32_t>(arg_type) & static_cast<std::uint32_t>(ipc_arg_type::flag_des))) {
            return epoc::error_argument;
        }

        eka2l1::ptr<std::uint8_t> param_ptr(msg->args.args[param]);
        std::uint8_t *param_ptr_host = param_ptr.get(msg->own_thr->owning_process());

        if (!param_ptr_host || !info_host) {
            return epoc::error_argument;
        }

        const std::int32_t result = do_ipc_manipulation(kern, msg->own_thr, param_ptr_host, *info_host, start_offset);
        msg->unref();

        return result;
    }

    BRIDGE_FUNC(std::int32_t, message_client, kernel::handle h, kernel::owner_type owner) {
        eka2l1::ipc_msg_ptr msg = kern->get_msg(h);

        if (!msg) {
            return epoc::error_bad_handle;
        }

        kernel::thread *msg_thr = msg->own_thr;
        return kern->open_handle(msg_thr, owner);
    }

    BRIDGE_FUNC(std::int32_t, message_open_handle, kernel::handle h, kernel::object_type obj_type,
        const std::int32_t index, kernel::owner_type owner) {
        eka2l1::ipc_msg_ptr msg = kern->get_msg(h);

        if (!msg) {
            return epoc::error_bad_handle;
        }

        if ((index < 0) || (index > 3)) {
            return epoc::error_argument;
        }

        auto obj = kern->get_kernel_obj_raw(static_cast<kernel::handle>(msg->args.args[index]), msg->own_thr);
        if (!obj) {
            return epoc::error_bad_handle;
        }

        if (obj->get_object_type() != obj_type) {
            LOG_ERROR(KERNEL, "Mismatch object type expected!");
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(kern->open_handle(obj, owner));
    }

    static void query_security_info(kernel::process *process, epoc::security_info *info) {
        assert(process);

        *info = std::move(process->get_sec_info());
    }

    BRIDGE_FUNC(void, process_security_info, kernel::handle h, eka2l1::ptr<epoc::security_info> info) {
        epoc::security_info *sec_info = info.get(kern->crr_process());
        process_ptr pr = kern->get<kernel::process>(h);

        query_security_info(pr, sec_info);
    }

    BRIDGE_FUNC(void, thread_security_info, kernel::handle h, eka2l1::ptr<epoc::security_info> info) {
        epoc::security_info *sec_info = info.get(kern->crr_process());

        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            LOG_ERROR(KERNEL, "Thread handle invalid 0x{:x}", h);
            return;
        }

        query_security_info(thr->owning_process(), sec_info);
    }

    BRIDGE_FUNC(void, message_security_info, std::int32_t h, eka2l1::ptr<epoc::security_info> info) {
        epoc::security_info *sec_info = info.get(kern->crr_process());
        eka2l1::ipc_msg_ptr msg = kern->get_msg(h);

        if (!msg) {
            LOG_ERROR(KERNEL, "Thread handle invalid 0x{:x}", h);
            return;
        }

        query_security_info(msg->own_thr->owning_process(), sec_info);
    }

    BRIDGE_FUNC(void, creator_security_info, epoc::security_info *info) {
        if (!info) {
            return;
        }

        kernel::process *crr_process = kern->crr_process();
        kernel::process *owner = crr_process->get_parent_process();

        if (!owner) {
            LOG_TRACE(KERNEL, "Process is a wild child, has no parents. Creator info is empty.");
            *info = epoc::security_info{};
        } else {
            *info = std::move(owner->get_sec_info());
        }
    }

    BRIDGE_FUNC(std::int32_t, session_security_info, std::int32_t h, epoc::security_info *info) {
        service::session *ss = kern->get<service::session>(h);

        if (!info || !ss) {
            return epoc::error_bad_handle;
        }

        server_ptr svr = ss->get_server();
        kernel::thread *owner = svr->get_owner_thread();
        if (owner) {
            query_security_info(owner->owning_process(), info);
        } else {
            info->reset();
            info->secure_id = svr->get_owner_secure_uid();
            info->vendor_id = svr->get_owner_vendor_uid();

            std::fill(info->caps_u, info->caps_u + epoc::security_info::total_caps_u_size, 0xFFFFFFFF);
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, server_create, eka2l1::ptr<desc8> server_name_des, std::int32_t mode) {
        kernel::process *crr_pr = kern->crr_process();

        std::string server_name = server_name_des.get(crr_pr)->to_std_string(crr_pr);


        if (!server_name.empty() && server_name[0] == '!') {
            if (!crr_pr->satisfy(server_exclamation_point_name_policy)) {
                return epoc::error_permission_denied;
            }
        }

        kernel::owner_type handle_mode = (server_name.empty()) ? kernel::owner_type::process : kernel::owner_type::thread;
        auto handle = kern->create_and_add<service::server>(handle_mode, kern->get_system(), kern->crr_thread(), server_name,
                              false, false, static_cast<service::share_mode>(mode))
                          .first;

        return handle;
    }

    BRIDGE_FUNC(void, server_receive, kernel::handle h, eka2l1::ptr<epoc::request_status> req_sts, eka2l1::ptr<void> data_ptr) {
        server_ptr server = kern->get<service::server>(h);

        if (!server) {
            return;
        }

        if (kern->get_config()->log_ipc)
            LOG_TRACE(KERNEL, "Receive requested from {}", server->name());

        server->receive_async_lle(req_sts, data_ptr.cast<service::message2>());
    }

    BRIDGE_FUNC(void, server_cancel, kernel::handle h) {
        server_ptr server = kern->get<service::server>(h);

        if (!server) {
            return;
        }

        server->cancel_async_lle();
    }

    static kernel::owner_type get_session_owner_type_from_share(const service::share_mode shmode) {
        if (shmode == service::SHARE_MODE_UNSHAREABLE) {
            return kernel::owner_type::thread;
        }

        return kernel::owner_type::process;
    }

    static std::int32_t do_create_session_from_server(kernel_system *kern, server_ptr server, std::int32_t msg_slot_count, eka2l1::ptr<void> sec, std::int32_t mode) {
        const service::share_mode sv_share = server->get_share_mode();

        if (sv_share < mode) {
            LOG_ERROR(KERNEL, "Share mode of session is not eligible to be on the server!");
            return (sv_share == service::SHARE_MODE_UNSHAREABLE) ? epoc::error_access_denied : epoc::error_permission_denied;
        }

        auto session_and_handle = kern->create_and_add<service::session>(
            get_session_owner_type_from_share(static_cast<service::share_mode>(mode)), server, msg_slot_count);

        if (session_and_handle.first == kernel::INVALID_HANDLE) {
            return epoc::error_general;
        }

        config::state *config = kern->get_config();

        if (config && config->log_ipc) {
            LOG_TRACE(KERNEL, "New session connected to {} with handle {}", server->name(), session_and_handle.first);
        }

        session_and_handle.second->set_associated_handle(session_and_handle.first);

        service::session *ss = session_and_handle.second;
        ss->set_share_mode(static_cast<service::share_mode>(mode));

        return session_and_handle.first;
    }

    BRIDGE_FUNC(std::int32_t, session_create, eka2l1::ptr<desc8> server_name_des, std::int32_t msg_slot, eka2l1::ptr<void> sec, std::int32_t mode) {
        process_ptr pr = kern->crr_process();

        const std::string server_name = server_name_des.get(pr)->to_std_string(pr);


        server_ptr server = kern->get_by_name<service::server>(server_name);

        if (!server) {
            LOG_TRACE(KERNEL, "Create session to unexist server: {}", server_name);
            return epoc::error_not_found;
        }

        return do_create_session_from_server(kern, server, msg_slot, sec, mode);
    }

    BRIDGE_FUNC(std::int32_t, session_create_from_handle, kernel::handle h, std::int32_t msg_slots, eka2l1::ptr<void> sec, std::int32_t mode) {
        process_ptr pr = kern->crr_process();
        server_ptr server = kern->get<service::server>(h);

        if (!server) {
            LOG_TRACE(KERNEL, "Create session to unexist server handle: {}", h);
            return epoc::error_not_found;
        }

        return do_create_session_from_server(kern, server, msg_slots, sec, mode);
    }

    BRIDGE_FUNC(std::int32_t, session_share, std::uint32_t *handle, std::int32_t share) {
        session_ptr ss = kern->get<service::session>(*handle);

        if (!ss) {
            return epoc::error_bad_handle;
        }

        const service::share_mode sv_share = ss->get_server()->get_share_mode();

        if (share > sv_share) {
            LOG_ERROR(KERNEL, "Share mode to set is not eligible to be on the server!");
            return (sv_share == service::SHARE_MODE_UNSHAREABLE) ? epoc::error_access_denied : epoc::error_permission_denied;
        }

        const service::share_mode prev_share = ss->get_share_mode();
        ss->set_share_mode(static_cast<service::share_mode>(share));

        if ((prev_share >= service::share_mode::SHARE_MODE_SHAREABLE) && (share == service::share_mode::SHARE_MODE_UNSHAREABLE)
            || ((prev_share == service::share_mode::SHARE_MODE_UNSHAREABLE) && (share >= service::SHARE_MODE_SHAREABLE))) {
            const std::uint32_t res = kern->mirror(ss, get_session_owner_type_from_share(static_cast<service::share_mode>(share)));
            if (res == kernel::INVALID_HANDLE) {
                LOG_ERROR(KERNEL, "Handle slots exhausted");
                return epoc::error_general;
            }

            kern->close(*handle);
            *handle = res;
        }

        return epoc::error_none;
    }

    static std::int32_t session_send_general(kernel_system *kern, kernel::handle h, std::int32_t ord, const std::uint32_t *ipc_args,
        eka2l1::ptr<epoc::request_status> status, const bool no_header_flag, const bool sync) {
        process_ptr crr_pr = kern->crr_process();

        ipc_arg arg;
        if (ipc_args) {
            if (no_header_flag) {
                arg.flag = 0xFFFFFFFF;
                std::memcpy(arg.args, ipc_args, sizeof(arg.args));
            } else {
                for (uint8_t i = 0; i < 4; i++) {
                    arg.args[i] = *ipc_args++;
                }

                arg.flag = *ipc_args & (((1 << 12) - 1) | (int)ipc_arg_pin::pin_mask);
            }
        } else {
            std::fill(arg.args, arg.args + sizeof(arg.args) / sizeof(std::uint32_t), 0);
        }

        session_ptr ss = kern->get<service::session>(h);

        if (!ss) {
            return epoc::error_bad_handle;
        }

        if (ss->is_server_terminated()) {
            return epoc::error_server_terminated;
        }

        if (!status) {
            LOG_TRACE(KERNEL, "Sending a blind sync message");
        }

        if (kern->get_config()->log_ipc) {
            LOG_TRACE(KERNEL, "Sending {} sync to {}", ord, ss->get_server()->name());
        }

        const std::string server_name = ss->get_server()->name();
        kern->call_ipc_send_callbacks(server_name, ord, arg, status.ptr_address(), kern->crr_thread());

        const int result = sync ? ss->send_receive_sync(ord, arg, status) : ss->send_receive(ord, arg, status);

        if (ss->get_server()->is_hle()) {
            ss->get_server()->process_accepted_msg();
        }

        return result;
    }

    BRIDGE_FUNC(std::int32_t, session_send_sync, kernel::handle h, std::int32_t ord, const std::uint32_t *ipc_args,
        eka2l1::ptr<epoc::request_status> status) {
        return session_send_general(kern, h, ord, ipc_args, status, false, true);
    }

    BRIDGE_FUNC(std::int32_t, session_send, kernel::handle h, std::int32_t ord, const std::uint32_t *ipc_args,
        eka2l1::ptr<epoc::request_status> status) {
        return session_send_general(kern, h, ord, ipc_args, status, false, false);
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, leave_start) {
        kernel::thread *thr = kern->crr_thread();
        LOG_TRACE(KERNEL, "Leave started! Guess leave code: {}", static_cast<std::int32_t>(kern->get_cpu()->get_reg(0)));

        thr->increase_leave_depth();

        return current_local_data(kern)->trap_handler;
    }

    BRIDGE_FUNC(void, leave_end) {
        kernel::thread *thr = kern->crr_thread();
        thr->decrease_leave_depth();

        if (thr->is_invalid_leave()) {
            LOG_CRITICAL(KERNEL, "Invalid leave, leave depth is negative!");
        }

        LOG_TRACE(KERNEL, "Leave trapped by trap handler.");
    }

    BRIDGE_FUNC(std::int32_t, debug_mask) {
        return 0;
    }

    BRIDGE_FUNC(void, set_debug_mask, std::int32_t ts_debug_mask) {
    }

    BRIDGE_FUNC(std::int32_t, debug_mask_index, std::int32_t idx) {
        return 0;
    }

    BRIDGE_FUNC(std::int32_t, hal_function, std::int32_t cage, std::int32_t func, eka2l1::ptr<std::int32_t> a1, eka2l1::ptr<std::int32_t> a2) {
        process_ptr pr = kern->crr_process();

        int *arg1 = a1.get(pr);
        int *arg2 = a2.get(pr);


        return do_hal(kern->get_system(), cage, func, arg1, arg2);
    }

    BRIDGE_FUNC(std::int32_t, chunk_new, epoc::owner_type owner, eka2l1::ptr<desc8> name_des,
        eka2l1::ptr<epoc::chunk_create> chunk_create_info_ptr) {
        memory_system *mem = kern->get_memory_system();
        process_ptr pr = kern->crr_process();

        epoc::chunk_create create_info = *chunk_create_info_ptr.get(pr);
        desc8 *name = name_des.get(pr);

        kernel::chunk_type type = kernel::chunk_type::normal;
        kernel::chunk_access access = kernel::chunk_access::local;
        kernel::chunk_attrib att = decltype(att)::none;
        prot perm = prot_read_write;

        if (create_info.att & epoc::chunk_create::disconnected) {
            type = kernel::chunk_type::disconnected;
        } else if (create_info.att & epoc::chunk_create::double_ended) {
            type = kernel::chunk_type::double_ended;
        }

        if (create_info.att & epoc::chunk_create::global) {
            access = kernel::chunk_access::global;
        } else {
            access = kernel::chunk_access::local;
        }

        if (create_info.att & epoc::chunk_create::code) {
            perm = prot_read_write_exec;
        }

        if ((access == decltype(access)::global) && ((!name) || (name->get_length() == 0))) {
            att = kernel::chunk_attrib::anonymous;
        }

        const kernel::handle h = kern->create_and_add<kernel::chunk>(
                                         owner == epoc::owner_process ? kernel::owner_type::process : kernel::owner_type::thread,
                                         mem, kern->crr_process(), name ? name->to_std_string(kern->crr_process()) : "", create_info.initial_bottom,
                                         create_info.initial_top, create_info.max_size, perm, type, access, att, create_info.clear_bytes)
                                     .first;

        if (h == kernel::INVALID_HANDLE) {
            return epoc::error_no_memory;
        }

        return h;
    }

    BRIDGE_FUNC(std::int32_t, chunk_max_size, kernel::handle h) {
        chunk_ptr chunk = kern->get<kernel::chunk>(h);
        if (!chunk) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(chunk->max_size());
    }

    BRIDGE_FUNC(eka2l1::ptr<std::uint8_t>, chunk_base, kernel::handle h) {
        chunk_ptr chunk = kern->get<kernel::chunk>(h);
        if (!chunk) {
            return 0;
        }

        return chunk->base(kern->crr_process());
    }

    BRIDGE_FUNC(std::int32_t, chunk_size, kernel::handle h) {
        chunk_ptr chunk = kern->get<kernel::chunk>(h);

        if (!chunk) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(chunk->committed());
    }

    BRIDGE_FUNC(std::int32_t, chunk_bottom, kernel::handle h) {
        chunk_ptr chunk = kern->get<kernel::chunk>(h);

        if (!chunk) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(chunk->bottom_offset());
    }

    BRIDGE_FUNC(std::int32_t, chunk_top, kernel::handle h) {
        chunk_ptr chunk = kern->get<kernel::chunk>(h);

        if (!chunk) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(chunk->top_offset());
    }

    BRIDGE_FUNC(std::int32_t, chunk_adjust, kernel::handle h, std::int32_t type, std::int32_t a1, std::int32_t a2) {
        chunk_ptr chunk = kern->get<kernel::chunk>(h);

        if (!chunk) {
            return epoc::error_bad_handle;
        }

        switch (type) {
        case 0:
            return (chunk->adjust(a1) ? epoc::error_none : epoc::error_no_memory);

        case 1:
            return (chunk->adjust_de(a1, a2) ? epoc::error_none : epoc::error_no_memory);

        case 2:
            return chunk->commit_symbian_compat(a1, a2);

        case 3:
            return (chunk->decommit(a1, a2) ? epoc::error_none : epoc::error_general);

        case 4: {
            std::int32_t result = chunk->allocate(a1);
            if (result < 0) {
                return epoc::error_no_memory;
            }

            return result;
        }

        case 5:
        case 6:
            return epoc::error_none;
        }

        return epoc::error_general;
    }

    BRIDGE_FUNC(void, imb_range, eka2l1::ptr<void> addr, std::uint32_t size) {
        process_ptr crr_process = kern->crr_process();
        void *addr_space_ptr = crr_process->get_ptr_on_addr_space(addr.ptr_address());

        if (addr_space_ptr && (size <= 0x100000)) {
            kern->run_imb_range_callback(crr_process, addr.ptr_address(), size);

            if (kern->get_config()->dump_imb_range_code) {
                auto start = std::chrono::system_clock::now();
                std::time_t end_time = std::chrono::system_clock::to_time_t(start);

                tm local_tm = *std::localtime(&end_time);

                const std::string filename = fmt::format("imb_{}_0x{:X}_{}_{}_{}_{}_{}_{}.dump", crr_process->unique_id(),
                    addr.ptr_address(), local_tm.tm_year + 1900, local_tm.tm_mon + 1, local_tm.tm_mday, local_tm.tm_hour, local_tm.tm_min,
                    local_tm.tm_sec);

                std::ofstream out(filename, std::ios_base::binary | std::ios_base::out);
                out.write(reinterpret_cast<char *>(addr_space_ptr), size);
            }
        }

        kern->get_cpu()->imb_range(addr.ptr_address(), size);
    }

    BRIDGE_FUNC(std::int32_t, semaphore_create, eka2l1::ptr<desc8> sema_name_des, std::int32_t init_count, epoc::owner_type owner) {
        process_ptr pr = kern->crr_process();

        desc8 *desname = sema_name_des.get(pr);
        kernel::owner_type owner_kern = (owner == epoc::owner_process) ? kernel::owner_type::process : kernel::owner_type::thread;

        const kernel::handle sema = kern->create_and_add<kernel::semaphore>(owner_kern, pr, !desname ? "" : desname->to_std_string(pr).c_str(),
                                            init_count, !desname ? kernel::access_type::local_access : kernel::access_type::global_access)
                                        .first;

        if (sema == kernel::INVALID_HANDLE) {
            return epoc::error_general;
        }

        return sema;
    }

    BRIDGE_FUNC(std::int32_t, semaphore_wait, kernel::handle h, std::int32_t timeout) {
        sema_ptr sema = kern->get<kernel::semaphore>(h);

        if (!sema) {
            return epoc::error_bad_handle;
        }

        sema->wait(kern->is_eka1() ? 0 : timeout);
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, semaphore_count, kernel::handle h) {
        sema_ptr sema = kern->get<kernel::semaphore>(h);

        if (!sema) {
            return eka2l1::epoc::error_bad_handle;
        }

        return sema->count();
    }

    BRIDGE_FUNC(void, semaphore_signal, kernel::handle h) {
        sema_ptr sema = kern->get<kernel::semaphore>(h);

        if (!sema) {
            return;
        }

        sema->signal(1);
    }

    BRIDGE_FUNC(void, semaphore_signal_n, kernel::handle h, std::int32_t sig_count) {
        sema_ptr sema = kern->get<kernel::semaphore>(h);

        if (!sema) {
            return;
        }

        sema->signal(sig_count);
    }

    BRIDGE_FUNC(std::int32_t, mutex_create, eka2l1::ptr<desc8> mutex_name_des, epoc::owner_type owner) {
        process_ptr pr = kern->crr_process();

        desc8 *desname = mutex_name_des.get(pr);
        kernel::owner_type owner_kern = (owner == epoc::owner_process) ? kernel::owner_type::process : kernel::owner_type::thread;

        const kernel::handle mut = kern->create_and_add<kernel::mutex>(owner_kern, kern->get_ntimer(), pr,
                                           !desname ? "" : desname->to_std_string(pr), false,
                                           !desname ? kernel::access_type::local_access : kernel::access_type::global_access)
                                       .first;

        if (mut == kernel::INVALID_HANDLE) {
            return epoc::error_general;
        }

        return mut;
    }

    BRIDGE_FUNC(std::int32_t, mutex_wait, kernel::handle h) {
        mutex_ptr mut = kern->get<kernel::mutex>(h);

        if (!mut || mut->get_object_type() != kernel::object_type::mutex) {
            return epoc::error_bad_handle;
        }

        mut->wait(kern->crr_thread());
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, mutex_wait_ver2, kernel::handle h, std::int32_t timeout) {
        mutex_ptr mut = kern->get<kernel::mutex>(h);

        if (!mut || mut->get_object_type() != kernel::object_type::mutex) {
            return epoc::error_bad_handle;
        }

        if (timeout == 0) {
            mut->wait(kern->crr_thread());
            return epoc::error_none;
        }

        if (timeout == -1) {
            mut->try_wait(kern->crr_thread());
            return epoc::error_none;
        }

        mut->wait_for(timeout);
        return epoc::error_none;
    }

    BRIDGE_FUNC(void, mutex_signal, kernel::handle h) {
        mutex_ptr mut = kern->get<kernel::mutex>(h);

        if (!mut || mut->get_object_type() != kernel::object_type::mutex) {
            return;
        }

        mut->signal(kern->crr_thread());
    }

    BRIDGE_FUNC(std::int32_t, mutex_is_held, kernel::handle h) {
        if (kern->is_eka1()) {
            LOG_ERROR(KERNEL, "EKA2 mutex behaviour is invalidly being invoked on EKA1!");
            return epoc::error_not_supported;
        }

        mutex_ptr mut = kern->get<kernel::mutex>(h);

        if (!mut || mut->get_object_type() != kernel::object_type::mutex) {
            return epoc::error_not_found;
        }

        const bool result = (mut->holder() == kern->crr_thread());

        return result;
    }

    BRIDGE_FUNC(std::int32_t, condvar_create, eka2l1::ptr<desc8> mutex_name_des, epoc::owner_type owner) {
        process_ptr pr = kern->crr_process();

        desc8 *desname = mutex_name_des.get(pr);
        kernel::owner_type owner_kern = (owner == epoc::owner_process) ? kernel::owner_type::process : kernel::owner_type::thread;

        const kernel::handle cv = kern->create_and_add<kernel::condvar>(owner_kern, kern->get_ntimer(), pr,
                                          !desname ? "" : desname->to_std_string(pr),
                                          !desname ? kernel::access_type::local_access : kernel::access_type::global_access)
                                      .first;

        if (cv == kernel::INVALID_HANDLE) {
            return epoc::error_general;
        }

        return cv;
    }

    BRIDGE_FUNC(std::int32_t, condvar_wait, kernel::handle condvar_h, kernel::handle mutex_h, std::int32_t timeout) {
        kernel::condvar *cv = kern->get<kernel::condvar>(condvar_h);
        kernel::mutex *mut = kern->get<kernel::mutex>(mutex_h);

        if (!cv || !mut) {
            return epoc::error_bad_handle;
        }

        return (cv->wait(kern->crr_thread(), mut, timeout) ? epoc::error_none : epoc::error_general);
    }

    BRIDGE_FUNC(void, condvar_signal, kernel::handle condvar_h) {
        kernel::condvar *cv = kern->get<kernel::condvar>(condvar_h);
        if (cv) {
            cv->signal();
        } else {
            LOG_ERROR(KERNEL, "Condition variable is null (handle={})", condvar_h);
        }
    }

    BRIDGE_FUNC(void, condvar_broadcast, kernel::handle condvar_h) {
        kernel::condvar *cv = kern->get<kernel::condvar>(condvar_h);
        if (cv) {
            cv->broadcast();
        } else {
            LOG_ERROR(KERNEL, "Condition variable is null (handle={})", condvar_h);
        }
    }

    BRIDGE_FUNC(void, wait_for_any_request) {
        kern->crr_thread()->wait_for_any_request();
    }

    BRIDGE_FUNC(void, request_signal, std::int32_t signal_count) {
        kern->crr_thread()->signal_request(signal_count);
    }

    BRIDGE_FUNC(std::int32_t, object_next, std::int32_t obj_type, eka2l1::ptr<des8> name_des, eka2l1::ptr<epoc::find_handle> handle_finder) {
        process_ptr pr = kern->crr_process();

        epoc::find_handle *handle = handle_finder.get(pr);
        epoc::des8 *name_des_ptr = name_des.get(pr);

        const std::string name = name_des_ptr->to_std_string(pr);

        if (handle->handle < 0) {
            return epoc::error_argument;
        }

        std::optional<eka2l1::find_handle> info = kern->find_object(name, handle->handle,
            static_cast<kernel::object_type>(obj_type), true);

        if (!info) {
            name_des_ptr->set_length(pr, 0);
            return epoc::error_not_found;
        }

        handle->handle = info->index;
        handle->obj_id_low = static_cast<std::uint32_t>(info->object_id);
        handle->obj_id_high = static_cast<std::uint32_t>(info->object_id >> 32);

        std::string the_full_name;
        info->obj->full_name(the_full_name);

        name_des_ptr->assign(pr, the_full_name);

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, handle_close, kernel::handle h) {
        if (h & 0x8000) {
            return epoc::error_general;
        }

        return kern->close(h);
    }

    BRIDGE_FUNC(std::int32_t, handle_duplicate, std::int32_t h, epoc::owner_type owner, std::int32_t dup_handle) {
        return kern->mirror(&(*kern->get<kernel::thread>(h)), dup_handle,
            (owner == epoc::owner_process) ? kernel::owner_type::process : kernel::owner_type::thread);
    }

    BRIDGE_FUNC(std::int32_t, handle_duplicate_v2, std::int32_t h, epoc::owner_type owner, std::uint32_t *dup_handle) {
        const std::uint32_t handle_result = kern->mirror(&(*kern->get<kernel::thread>(h)), *dup_handle,
            (owner == epoc::owner_process) ? kernel::owner_type::process : kernel::owner_type::thread);

        if (handle_result == kernel::INVALID_HANDLE) {
            return epoc::error_not_found;
        }

        *dup_handle = handle_result;
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, handle_open_object, std::int32_t obj_type, eka2l1::ptr<epoc::desc8> name_des, std::int32_t owner) {
        process_ptr pr = kern->crr_process();
        std::string obj_name = name_des.get(pr)->to_std_string(pr);

        kernel_obj_ptr obj = kern->get_by_name_and_type<kernel::kernel_obj>(obj_name,
            static_cast<kernel::object_type>(obj_type));

        if (!obj) {
            LOG_ERROR(KERNEL, "Can't open object: {}", obj_name);
            return epoc::error_not_found;
        }

        kernel::handle ret_handle = kern->mirror(obj, static_cast<eka2l1::kernel::owner_type>(owner));

        if (ret_handle != kernel::INVALID_HANDLE) {
            return ret_handle;
        }

        LOG_ERROR(KERNEL, "Unable to instantiate handle for object opening with owner={}", owner);
        return epoc::error_general;
    }

    BRIDGE_FUNC(std::int32_t, handle_open_object_by_find_handle, std::int32_t owner, epoc::find_handle *finder) {
        if (!finder) {
            LOG_ERROR(KERNEL, "Find handle object pointer is null!");
            return epoc::error_argument;
        }

        kernel_obj_ptr obj = kern->get_object_from_find_handle(finder->handle);
        if (!obj) {
            LOG_ERROR(KERNEL, "Can't open object with find handle: 0x{:X}", finder->handle);
            return epoc::error_not_found;
        }

        kernel::handle ret_handle = kern->mirror(obj, static_cast<eka2l1::kernel::owner_type>(owner));

        if (ret_handle != kernel::INVALID_HANDLE) {
            return ret_handle;
        }

        LOG_ERROR(KERNEL, "Unable to instantiate handle for object opening with owner={}", owner);
        return epoc::error_general;
    }

    BRIDGE_FUNC(void, handle_name, kernel::handle h, eka2l1::ptr<des8> name_des) {
        kernel_obj_ptr obj = kern->get_kernel_obj_raw(h, kern->crr_thread());
        process_ptr crr_pr = kern->crr_process();

        if (!obj) {
            return;
        }

        des8 *desname = name_des.get(crr_pr);
        desname->assign(crr_pr, obj->name());
    }

    BRIDGE_FUNC(void, handle_full_name, kernel::handle h, eka2l1::ptr<des8> full_name_des) {
        kernel_obj_ptr obj = kern->get_kernel_obj_raw(h, kern->crr_thread());
        process_ptr pr = kern->crr_process();

        if (!obj) {
            return;
        }

        des8 *desname = full_name_des.get(pr);

        std::string full_name;
        obj->full_name(full_name);

        desname->assign(pr, full_name);
    }

    BRIDGE_FUNC(void, handle_info, const kernel::handle h, eka2l1::ptr<kernel::handle_info> info) {
        kernel_obj_ptr the_object = kern->get_kernel_obj_raw(h, kern->crr_thread());
        kernel::process *crr_pr = kern->crr_process();

        if (!the_object) {
            LOG_WARN(KERNEL, "Trying to get handle info of an invalid handle: 0x{:X}, ignored", h);
            return;
        }

        kernel::handle_info *info_ptr = info.get(crr_pr);
        if (!info_ptr) {
            LOG_WARN(KERNEL, "Trying to get handle info but the information struct is null!");
            return;
        }

        kern->get_info(the_object, *info_ptr);
    }

    BRIDGE_FUNC(void, handle_count, kernel::handle h, std::uint32_t *total_handle_process, std::uint32_t *total_handle_thread) {
        kernel::thread *thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return;
        }

        *total_handle_thread = static_cast<std::uint32_t>(thr->get_total_open_handles());
        *total_handle_process = static_cast<std::uint32_t>(thr->owning_process()->get_total_open_handles());
    }

    std::int32_t get_call_list_from_codeseg(codeseg_ptr cs, process_ptr caller, std::int32_t *count, address *list_ep) {
        std::vector<uint32_t> list;
        cs->queries_call_list(caller, list);
        cs->unmark();

        *count = common::min<std::int32_t>(static_cast<std::int32_t>(list.size()), *count);
        memcpy(list_ep, list.data(), sizeof(std::uint32_t) * *count);

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, static_call_list, std::int32_t *total, std::uint32_t *list_ptr) {
        kernel::process *pr = kern->crr_process();
        kernel::codeseg *seg = pr->get_codeseg();

        const std::int32_t res = get_call_list_from_codeseg(seg, pr, total, list_ptr);
        if ((res == 0) && (kern->get_epoc_version() < epocver::epoc10)) {
            seg->attached_report(pr);
        }

        return res;
    }

    BRIDGE_FUNC(void, static_call_done) {
        kernel::process *pr = kern->crr_process();
        kernel::codeseg *seg = pr->get_codeseg();

        seg->attached_report(pr);
    }

    BRIDGE_FUNC(std::int32_t, wait_dll_lock) {
        kern->crr_process()->wait_dll_lock();
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, release_dll_lock) {
        kern->crr_process()->signal_dll_lock(kern->crr_thread());

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, library_attach, kernel::handle h, eka2l1::ptr<std::int32_t> num_eps, eka2l1::ptr<std::uint32_t> ep_list) {
        library_ptr lib = kern->get<kernel::library>(h);

        if (!lib) {
            return epoc::error_bad_handle;
        }

        process_ptr pr = kern->crr_process();

        std::vector<uint32_t> entries = lib->attach(kern->crr_process());
        const std::uint32_t num_to_copy = common::min<std::uint32_t>(*num_eps.get(pr),
            static_cast<std::uint32_t>(entries.size()));

        *num_eps.get(pr) = num_to_copy;

        address *episode_choose_your_story = nullptr;

        if (kern->is_eka1()) {
            eka2l1::ptr<address> *arr_ptr = ep_list.cast<eka2l1::ptr<address>>().get(pr);
            if (!arr_ptr) {
                LOG_ERROR(KERNEL, "Entry point list for libraries has invalid address = 0x{:X}", ep_list.ptr_address());
                return epoc::error_argument;
            }

            episode_choose_your_story = arr_ptr->get(pr);
        } else {
            episode_choose_your_story = ep_list.cast<address>().get(pr);
        }

        if (!episode_choose_your_story) {
            LOG_ERROR(KERNEL, "Entry point list for libraries has invalid address = 0x{:X}", ep_list.ptr_address());
            return epoc::error_argument;
        }

        std::memcpy(episode_choose_your_story, entries.data(), num_to_copy * sizeof(address));

        if (kern->is_eka1()) {
            lib->attached(pr);
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, library_lookup, kernel::handle h, std::uint32_t ord_index) {
        library_ptr lib = kern->get<kernel::library>(h);

        if (!lib) {
            return 0;
        }

        std::optional<uint32_t> func_addr = lib->get_ordinal_address(kern->crr_process(),
            ord_index);

        if (!func_addr) {
            return 0;
        }

        return *func_addr;
    }

    BRIDGE_FUNC(std::int32_t, library_attached, kernel::handle h) {
        library_ptr lib = kern->get<kernel::library>(h);

        if (!lib) {
            return epoc::error_bad_handle;
        }

        bool attached_result = lib->attached(kern->crr_process());

        if (!attached_result) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, library_detach, std::int32_t *count, address *ep) {
        return kern->crr_thread()->get_detach_eps_limit(count, ep);
    }

    BRIDGE_FUNC(std::int32_t, library_detached) {
        kern->crr_thread()->cleanup_detachs();
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, library_get_memory_info, kernel::handle h, kernel::memory_info *info) {
        library_ptr lib = kern->get<kernel::library>(h);

        if (!lib) {
            return epoc::error_bad_handle;
        }

        if (!info) {
            return epoc::error_argument;
        }

        get_memory_info_from_codeseg(lib->get_codeseg(), kern->crr_process(), info);
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::uint32_t, library_load_prepare) {
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::uint32_t, library_entry_call_start, const address addr) {
        return epoc::error_none;
    }

    BRIDGE_FUNC(void, library_filename, kernel::handle h, epoc::des8 *path_to_fill) {
        kernel::library *lib = kern->get<kernel::library>(h);

        if (!lib) {
            return;
        }

        const std::u16string lib_filename = lib->get_codeseg()->get_full_path();
        path_to_fill->assign(kern->crr_process(), common::ucs2_to_utf8(lib_filename));
    }

    BRIDGE_FUNC(std::int32_t, library_entry_point, kernel::handle h) {
        kernel::library *lib = kern->get<kernel::library>(h);

        if (!lib) {
            return epoc::error_bad_handle;
        }

        return lib->get_codeseg()->get_entry_point(kern->crr_process());
    }

    BRIDGE_FUNC(std::int32_t, user_svr_rom_header_address) {
        return kern->get_rom_info()->header.rom_base;
    }

    BRIDGE_FUNC(std::int32_t, user_svr_rom_root_dir_address) {
        return kern->get_rom_info()->header.rom_root_dir_list;
    }

    struct thread_create_info_expand {
        int handle;
        int type;
        address func_ptr;
        address ptr;
        address supervisor_stack;
        int supervisor_stack_size;
        address user_stack;
        int user_stack_size;
        kernel::thread_priority init_thread_priority;
        ptr_desc<std::uint8_t> name;
        int total_size;

        address allocator;
        int heap_initial_size;
        int heap_max_size;
        int flags;
    };

    static_assert(sizeof(thread_create_info_expand) == 64,
        "Thread create info struct size invalid");

    BRIDGE_FUNC(std::int32_t, thread_create, eka2l1::ptr<desc8> thread_name_des, epoc::owner_type owner, eka2l1::ptr<thread_create_info_expand> info_ptr) {
        process_ptr pr = kern->crr_process();
        memory_system *mem = kern->get_memory_system();

        std::string thr_name = thread_name_des.get(pr)->to_std_string(pr).c_str();
        thread_create_info_expand *info = info_ptr.get(pr);

        if (thr_name.empty()) {
            thr_name = "AnonymousThread";
        }

        const kernel::handle thr_handle = kern->create_and_add<kernel::thread>(static_cast<kernel::owner_type>(owner),
                                                  mem, kern->get_ntimer(), kern->crr_process(),
                                                  kernel::access_type::local_access, thr_name, info->func_ptr, info->user_stack_size,
                                                  info->heap_initial_size, info->heap_max_size, false, info->ptr, info->allocator, kernel::thread_priority::priority_normal)
                                              .first;

        if (thr_handle == kernel::INVALID_HANDLE) {
            return epoc::error_general;
        } else {
            LOG_TRACE(KERNEL, "Thread {} created with start pc = 0x{:x}, stack size = 0x{:x}", thr_name,
                info->func_ptr, info->user_stack_size);
        }

        return thr_handle;
    }

    BRIDGE_FUNC(std::int32_t, last_thread_handle) {
        return kern->crr_thread()->last_handle();
    }

    BRIDGE_FUNC(std::int32_t, thread_id, kernel::handle h) {
        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(thr->unique_id());
    }

    BRIDGE_FUNC(std::int32_t, thread_kill, kernel::handle h, kernel::entity_exit_type etype, std::int32_t reason, eka2l1::ptr<desc8> reason_des) {
        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        std::string exit_category = "None";
        kernel::process *pr = kern->crr_process();

        if (reason_des) {
            exit_category = reason_des.get(pr)->to_std_string(pr);
        }

        thr->kill(etype, common::utf8_to_ucs2(exit_category), reason);
        return epoc::error_none;
    }

    BRIDGE_FUNC(void, thread_request_signal, kernel::handle h) {
        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            LOG_ERROR(KERNEL, "Thread not found with handle {}", h);
            return;
        }

        thr->signal_request();
    }

    BRIDGE_FUNC(std::int32_t, thread_rename, kernel::handle h, eka2l1::ptr<desc8> name_des) {
        process_ptr pr = kern->crr_process();

        thread_ptr thr = kern->get<kernel::thread>(h);
        desc8 *name = name_des.get(pr);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        std::string new_name = name->to_std_string(pr);

        LOG_TRACE(KERNEL, "Thread with last name: {} renamed to {}", thr->name(), new_name);

        thr->rename(new_name);
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, thread_process, kernel::handle h) {
        thread_ptr thr = kern->get<kernel::thread>(h);

        return kern->mirror(kern->get_by_id<kernel::process>(thr->owning_process()->unique_id()),
            kernel::owner_type::thread);
    }

    BRIDGE_FUNC(void, thread_set_priority, kernel::handle h, std::int32_t thread_pri) {
        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return;
        }

        thr->set_priority(static_cast<eka2l1::kernel::thread_priority>(thread_pri));
    }

    BRIDGE_FUNC(std::int32_t, thread_priority, kernel::handle h) {
        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        return static_cast<std::int32_t>(thr->get_priority());
    }

    BRIDGE_FUNC(void, thread_resume, kernel::handle h) {
        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            LOG_ERROR(KERNEL, "invalid thread handle 0x{:x}", h);
            return;
        }

        switch (thr->current_state()) {
        case kernel::thread_state::create: {
            kern->get_thread_scheduler()->schedule(&(*thr));
            break;
        }

        default: {
            thr->resume();
            break;
        }
        }
    }

    BRIDGE_FUNC(void, thread_suspend, kernel::handle h) {
        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            LOG_ERROR(KERNEL, "invalid thread handle 0x{:x}", h);
            return;
        }

        switch (thr->current_state()) {
        case kernel::thread_state::create: {
            break;
        }

        default: {
            thr->suspend();
            break;
        }
        }
    }

    BRIDGE_FUNC(void, thread_rendezvous, std::int32_t reason) {
        kern->crr_thread()->rendezvous(reason);
    }

    BRIDGE_FUNC(void, thread_logon, kernel::handle h, eka2l1::ptr<epoc::request_status> req_sts, bool rendezvous) {
        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return;
        }

        thr->logon(req_sts, rendezvous);
    }

    BRIDGE_FUNC(std::int32_t, thread_logon_cancel, kernel::handle h, eka2l1::ptr<epoc::request_status> req_sts,
        bool rendezvous) {
        thread_ptr thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        bool logon_success = thr->logon_cancel(req_sts, rendezvous);

        if (logon_success) {
            return epoc::error_none;
        }

        return epoc::error_general;
    }

    BRIDGE_FUNC(void, thread_set_flags, kernel::handle h, std::uint32_t clear_mask, std::uint32_t set_mask) {
        thread_ptr thr = kern->get<kernel::thread>(h);

        uint32_t org_flags = thr->get_flags();
        uint32_t new_flags = ((org_flags & ~clear_mask) | set_mask);

        thr->set_flags(org_flags ^ new_flags);
    }

    BRIDGE_FUNC(std::int32_t, thread_open_by_id, const std::uint32_t id, const epoc::owner_type owner) {
        kernel::thread *thr = kern->get_by_id<kernel::thread>(id);

        if (!thr) {
            LOG_ERROR(KERNEL, "Unable to find thread with ID: {}", id);
            return epoc::error_not_found;
        }

        const kernel::handle h = kern->open_handle(thr, static_cast<kernel::owner_type>(owner));

        if (h == kernel::INVALID_HANDLE) {
            return epoc::error_general;
        }

        return static_cast<std::int32_t>(h);
    }

    BRIDGE_FUNC(std::int32_t, thread_exit_type, const kernel::handle h) {
        kernel::thread *thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return epoc::error_not_found;
        }

        return static_cast<std::int32_t>(thr->get_exit_type());
    }

    BRIDGE_FUNC(std::int32_t, thread_exit_reason, const kernel::handle h) {
        kernel::thread *thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return epoc::error_not_found;
        }

        return static_cast<std::int32_t>(thr->get_exit_reason());
    }

    BRIDGE_FUNC(std::int32_t, thread_request_count, const kernel::handle h) {
        kernel::thread *thr = kern->get<kernel::thread>(h);

        if (!thr) {
            LOG_ERROR(KERNEL, "Thread with handle 0x{:X} not found, returning 0", h);
            return 0;
        }

        return thr->request_count();
    }

    BRIDGE_FUNC(std::int32_t, thread_user_exiting, const std::int32_t reason) {
        return (kern->crr_process()->get_thread_count() == 1);
    }

    BRIDGE_FUNC(std::int32_t, thread_get_cpu_time, const kernel::handle h, std::uint64_t *time_run_in_us) {
        kernel::thread *thr = kern->get<kernel::thread>(h);

        if (!thr) {
            LOG_ERROR(KERNEL, "Thread with handle 0x{:X} not found!", h);
            return epoc::error_bad_handle;
        }

        if (!time_run_in_us) {
            return epoc::error_argument;
        }

        *time_run_in_us = thr->get_real_active_time();
        if (thr == kern->crr_thread()) {
            *time_run_in_us += kern->get_ntimer()->microseconds() - thr->get_real_time_active_begin();
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, thread_stack_info, const kernel::handle h, kernel::stack_info *info) {
        kernel::thread *thr = kern->get<kernel::thread>(h);

        if (!thr) {
            LOG_ERROR(KERNEL, "Thread with handle 0x{:X} not found!", h);
            return epoc::error_bad_handle;
        }

        if (!info) {
            return epoc::error_argument;
        }

        *info = thr->get_stack_info();
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, process_open_by_id, std::uint32_t id, const epoc::owner_type owner) {
        auto pr = kern->get_by_id<kernel::process>(id);

        if (!pr) {
            LOG_ERROR(KERNEL, "Unable to find process with ID: {}", id);
            return epoc::error_not_found;
        }

        const kernel::handle h = kern->open_handle(pr, static_cast<kernel::owner_type>(owner));

        if (h == kernel::INVALID_HANDLE) {
            return epoc::error_general;
        }

        return static_cast<std::int32_t>(h);
    }

    BRIDGE_FUNC(std::int32_t, property_find_get_int, std::int32_t cage, std::int32_t key, eka2l1::ptr<std::int32_t> value) {
        property_ptr prop = kern->get_prop(cage, key);


        if (!prop || !prop->is_defined()) {
            LOG_WARN(KERNEL, "Property not found: category = 0x{:x}, key = 0x{:x}", cage, key);
            return epoc::error_not_found;
        }

        std::int32_t *val_ptr = value.get(kern->crr_process());
        *val_ptr = prop->get_int();

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, property_find_get_bin, std::int32_t cage, std::int32_t key, eka2l1::ptr<std::uint8_t> data, std::int32_t datlength) {
        process_ptr crr_pr = kern->crr_process();

        property_ptr prop = kern->get_prop(cage, key);

        if (!prop || !prop->is_defined()) {
            LOG_WARN(KERNEL, "Property not found: category = 0x{:x}, key = 0x{:x}", cage, key);
            return epoc::error_not_found;
        }

        std::uint8_t *data_ptr = data.get(crr_pr);
        auto data_vec = prop->get_bin();

        const std::size_t size_to_copy = std::min<std::size_t>(data_vec.size(), datlength);
        std::int32_t return_code = epoc::error_none;

        if (data_vec.size() > datlength) {
            return_code = epoc::error_overflow;
        }

        std::copy(data_vec.begin(), data_vec.begin() + size_to_copy, data.get(kern->crr_process()));

        if (return_code != epoc::error_none) {
            return return_code;
        }

        return datlength;
    }

    BRIDGE_FUNC(std::int32_t, property_attach, std::int32_t cage, std::int32_t val, epoc::owner_type owner) {
        property_ptr prop = kern->get_prop(cage, val);

        LOG_TRACE(KERNEL, "Attach to property with category: 0x{:x}, key: 0x{:x}", cage, val);

        if (!prop) {
            LOG_WARN(KERNEL, "Property (0x{:x}, 0x{:x}) has not been defined before, undefined behavior may rise", cage, val);

            prop = kern->create<service::property>();

            if (!prop) {
                return epoc::error_general;
            }

            prop->first = cage;
            prop->second = val;
        }

        auto property_ref_handle_and_obj = kern->create_and_add<service::property_reference>(
            static_cast<kernel::owner_type>(owner), prop);

        if (property_ref_handle_and_obj.first == kernel::INVALID_HANDLE) {
            return epoc::error_general;
        }

        return property_ref_handle_and_obj.first;
    }

    BRIDGE_FUNC(std::int32_t, property_define, std::int32_t cage, std::int32_t key, eka2l1::ptr<epoc::property_info> prop_info_ptr) {
        process_ptr pr = kern->crr_process();

        epoc::property_info *info = prop_info_ptr.get(pr);

        service::property_type prop_type;

        switch (info->type) {
        case property_type_int:
            prop_type = service::property_type::int_data;
            break;

        case property_type_large_byte_array:
        case property_type_byte_array:
            prop_type = service::property_type::bin_data;
            break;

        default: {
            LOG_WARN(KERNEL, "Unknown property type, exit with epoc::error_general.");
            return epoc::error_argument;
        }
        }

        LOG_TRACE(KERNEL, "Define to property with category: 0x{:x}, key: 0x{:x}, type: {}", cage, key,
            prop_type == service::property_type::int_data ? "int" : "bin");

        property_ptr prop = kern->get_prop(cage, key);

        if (!prop) {
            prop = kern->create<service::property>();

            if (!prop) {
                return epoc::error_general;
            }

            prop->first = cage;
            prop->second = key;
        }

        prop->define(prop_type, info->size);

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, property_delete, std::int32_t cage, std::int32_t key) {
        property_ptr prop = kern->delete_prop(cage, key);

        if (!prop || !prop->is_defined()) {
            return epoc::error_not_found;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, property_subscribe, kernel::handle h, eka2l1::ptr<epoc::request_status> sts) {
        property_ref_ptr prop = kern->get<service::property_reference>(h);

        if (!prop) {
            return;
        }

        epoc::notify_info info(sts, kern->crr_thread());
        prop->subscribe(info);
    }

    BRIDGE_FUNC(void, property_cancel, kernel::handle h) {
        property_ref_ptr prop = kern->get<service::property_reference>(h);

        if (!prop) {
            return;
        }

        prop->cancel();

        return;
    }

    BRIDGE_FUNC(std::int32_t, property_set_int, kernel::handle h, std::int32_t val) {
        property_ref_ptr prop = kern->get<service::property_reference>(h);

        if (!prop || !prop->get_property_object()->is_defined()) {
            return epoc::error_not_found;
        }

        bool res = prop->get_property_object()->set_int(val);

        if (!res) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, property_set_bin, kernel::handle h, eka2l1::ptr<std::uint8_t> data_ptr, std::int32_t size) {
        property_ref_ptr prop = kern->get<service::property_reference>(h);

        if (!prop || !prop->get_property_object()->is_defined()) {
            return epoc::error_not_found;
        }

        bool res = prop->get_property_object()->set(data_ptr.get(kern->crr_process()), size);

        if (!res) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, property_get_int, kernel::handle h, eka2l1::ptr<std::int32_t> value_ptr) {
        process_ptr pr = kern->crr_process();
        property_ref_ptr prop = kern->get<service::property_reference>(h);

        if (!prop || !prop->get_property_object()->is_defined()) {
            return epoc::error_not_found;
        }

        *value_ptr.get(pr) = prop->get_property_object()->get_int();

        if (prop->get_property_object()->get_int() == -1) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, property_get_bin, kernel::handle h, eka2l1::ptr<std::uint8_t> buffer_ptr_guest, std::int32_t buffer_size) {
        property_ref_ptr prop = kern->get<service::property_reference>(h);

        if (!prop || !prop->get_property_object()->is_defined()) {
            return epoc::error_not_found;
        }

        std::vector<uint8_t> dat = prop->get_property_object()->get_bin();

        if (dat.size() == 0) {
            return epoc::error_argument;
        }

        const std::size_t size_to_copy = std::min<std::size_t>(dat.size(), buffer_size);
        std::int32_t return_code = epoc::error_none;

        if (dat.size() > buffer_size) {
            return_code = epoc::error_overflow;
        }

        std::copy(dat.begin(), dat.begin() + size_to_copy, buffer_ptr_guest.get(kern->crr_process()));

        if (return_code != epoc::error_none) {
            return return_code;
        }

        return buffer_size;
    }

    BRIDGE_FUNC(std::int32_t, property_find_set_int, std::int32_t cage, std::int32_t key, std::int32_t value) {
        property_ptr prop = kern->get_prop(cage, key);

        if (!prop || !prop->is_defined()) {
            return epoc::error_not_found;
        }

        const bool res = prop->set(value);

        if (!res) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, property_find_set_bin, std::int32_t cage, std::int32_t key, eka2l1::ptr<std::uint8_t> data_ptr, std::int32_t size) {
        property_ptr prop = kern->get_prop(cage, key);

        if (!prop || !prop->is_defined()) {
            return epoc::error_not_found;
        }

        const bool res = prop->set(data_ptr.get(kern->crr_process()), size);

        if (!res) {
            return epoc::error_argument;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, clear_inactivity_time) {
        kern->reset_inactivity_time();
    }

    BRIDGE_FUNC(std::uint32_t, get_inactivity_time) {
        return kern->inactivity_time();
    }

    BRIDGE_FUNC(std::int32_t, timer_create) {
        return kern->create_and_add<kernel::timer>(kernel::owner_type::process, kern->get_ntimer(),
                       "timer" + common::to_string(eka2l1::random()))
            .first;
    }

    BRIDGE_FUNC(void, timer_after, kernel::handle h, eka2l1::ptr<epoc::request_status> req_sts, std::int32_t us_after) {
        timer_ptr timer = kern->get<kernel::timer>(h);

        if (!timer) {
            return;
        }

        timer->after(kern->crr_thread(), req_sts, us_after);
    }

    BRIDGE_FUNC(void, timer_lock, kernel::handle h, eka2l1::ptr<epoc::request_status> req_sts, std::uint32_t second_fraction_enum) {
        timer_ptr timer = kern->get<kernel::timer>(h);

        if (!timer) {
            return;
        }

        second_fraction_enum += 1;

        if (second_fraction_enum >= 12) {
            second_fraction_enum = 12;
        }

        timer->after(kern->crr_thread(), req_sts, common::microsecs_per_sec * second_fraction_enum / 12);
    }

    BRIDGE_FUNC(void, timer_at_utc, kernel::handle h, eka2l1::ptr<epoc::request_status> req_sts, std::uint64_t *us_at) {
        timer_ptr timer = kern->get<kernel::timer>(h);
        epoc::request_status *sts_real = req_sts.get(kern->crr_process());

        if (!timer || !us_at || !sts_real) {
            return;
        }

        const std::uint64_t stamp = kern->universal_time();

        if (*us_at < stamp) {
            sts_real->set(epoc::error_underflow, kern->is_eka1());
            kern->crr_thread()->signal_request();

            return;
        }

        timer->after(kern->crr_thread(), req_sts, *us_at - stamp);
    }

    BRIDGE_FUNC(void, timer_cancel, kernel::handle h) {
        timer_ptr timer = kern->get<kernel::timer>(h);

        if (!timer) {
            return;
        }

        timer->cancel_request();
    }

    static constexpr std::uint32_t TICK_MASK = ~0b1;

    BRIDGE_FUNC(std::uint32_t, fast_counter) {
        const std::uint64_t DEFAULT_FAST_COUNTER_PERIOD = (common::microsecs_per_sec / epoc::HIGH_RES_TIMER_HZ);

        ntimer *timing = kern->get_ntimer();
        return static_cast<std::uint32_t>(timing->microseconds() / DEFAULT_FAST_COUNTER_PERIOD);
    }

    BRIDGE_FUNC(std::uint32_t, ntick_count) {
        const std::uint64_t DEFAULT_NTICK_PERIOD = (common::microsecs_per_sec / epoc::NANOKERNEL_HZ);

        ntimer *timing = kern->get_ntimer();
        return static_cast<std::uint32_t>(timing->microseconds() / DEFAULT_NTICK_PERIOD);
    }

    BRIDGE_FUNC(std::uint32_t, tick_count) {
        const std::uint64_t DEFAULT_TICK_PERIOD = (common::microsecs_per_sec / epoc::TICK_TIMER_HZ);

        ntimer *timing = kern->get_ntimer();
        std::uint32_t res = static_cast<std::uint32_t>((timing->microseconds() / DEFAULT_TICK_PERIOD));

        if (kern->is_eka1()) {
            res &= TICK_MASK;
        }

        return res;
    }

    BRIDGE_FUNC(std::int32_t, change_notifier_create, epoc::owner_type owner) {
        return kern->create_and_add<kernel::change_notifier>(static_cast<kernel::owner_type>(owner)).first;
    }

    BRIDGE_FUNC(std::int32_t, change_notifier_logon, kernel::handle h, eka2l1::ptr<epoc::request_status> req_sts) {
        change_notifier_ptr cnot = kern->get<kernel::change_notifier>(h);

        if (!cnot) {
            return epoc::error_bad_handle;
        }

        bool res = cnot->logon(req_sts);

        if (!res) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, change_notifier_logon_eka1, eka2l1::ptr<epoc::request_status> reqsts, kernel::handle h) {
        return change_notifier_logon(kern, h, reqsts);
    }

    BRIDGE_FUNC(std::int32_t, change_notifier_logoff, kernel::handle h) {
        change_notifier_ptr cnot = kern->get<kernel::change_notifier>(h);

        if (!cnot) {
            return epoc::error_bad_handle;
        }

        bool res = cnot->logon_cancel();

        if (!res) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, message_queue_notify_data_available, const kernel::handle h, eka2l1::ptr<epoc::request_status> sts) {
        kernel::thread *request_thread = kern->crr_thread();

        epoc::notify_info info;
        info.requester = request_thread;
        info.sts = sts;

        kernel::msg_queue *queue = kern->get<kernel::msg_queue>(h);

        if (!queue) {
            return epoc::error_bad_handle;
        }

        const bool result = queue->notify_available(info);

        if (!result) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, message_queue_notify_space_available, const kernel::handle h, eka2l1::ptr<epoc::request_status> sts) {
        kernel::thread *request_thread = kern->crr_thread();

        epoc::notify_info info;
        info.requester = request_thread;
        info.sts = sts;

        kernel::msg_queue *queue = kern->get<kernel::msg_queue>(h);

        if (!queue) {
            return epoc::error_bad_handle;
        }

        const bool result = queue->notify_free(info);

        if (!result) {
            return epoc::error_general;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, message_queue_cancel_notify_available, kernel::handle h) {
        kernel::thread *request_thread = kern->crr_thread();

        kernel::msg_queue *queue = kern->get<kernel::msg_queue>(h);

        if (!queue) {
            return;
        }

        queue->cancel_data_available(request_thread);
    }

    BRIDGE_FUNC(std::int32_t, message_queue_create, eka2l1::ptr<des8> name, const std::int32_t size,
        const std::int32_t length, epoc::owner_type owner) {
        if ((length <= 0) || (length > 256) | (size <= 0)) {
            return epoc::error_argument;
        }

        process_ptr cur_pr = kern->crr_process();
        std::string name_str;
        if (name.ptr_address()) {
            name_str = name.get(cur_pr)->to_std_string(cur_pr);
        } else {
            name_str = "";
        }

        auto queue_ptr = kern->create_and_add<kernel::msg_queue>(static_cast<kernel::owner_type>(owner), name_str, length, size).first;
        if (queue_ptr == kernel::INVALID_HANDLE) {
            return epoc::error_no_memory;
        }

        return queue_ptr;
    }

    BRIDGE_FUNC(std::int32_t, message_queue_send, kernel::handle h, void *data, const std::int32_t length) {
        kernel::msg_queue *queue = kern->get<kernel::msg_queue>(h);

        if (!queue) {
            return epoc::error_bad_handle;
        }

        if ((length <= 0) || (static_cast<std::size_t>(length) > queue->max_message_length())) {
            return epoc::error_argument;
        }

        if (!queue->send(data, length)) {
            return epoc::error_overflow;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, message_queue_receive, kernel::handle h, void *data, const std::int32_t length) {
        kernel::msg_queue *queue = kern->get<kernel::msg_queue>(h);

        if (!queue) {
            return epoc::error_bad_handle;
        }

        if (length != queue->max_message_length()) {
            LOG_ERROR(KERNEL, "Size of destination buffer vs size of queue mismatch ({} vs {})", length,
                queue->max_message_length());

            return epoc::error_argument;
        }

        if (!queue->receive(data, length)) {
            return epoc::error_underflow;
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(void, debug_print, desc8 *tdes, std::int32_t mode) {
        if (!tdes) {
            return;
        }

        LOG_TRACE(EMULATED_STDOUT, "{}", tdes->to_std_string(kern->crr_process()));
    }

    std::int32_t debug_command_do_read_write(kernel_system *kern, const std::uint32_t thread_id,
        const address addr, const address data_ptr, const std::int32_t len, const bool is_write) {
        if (len < 0) {
            LOG_ERROR(KERNEL, "Invalid length to write.");
            return epoc::error_argument;
        }

        kernel::process *crr = kern->crr_process();
        epoc::des8 *buf = eka2l1::ptr<epoc::des8>(data_ptr).get(crr);

        kernel::thread *thr_to_operate = kern->get_by_id<kernel::thread>(thread_id);
        if (!thr_to_operate || !buf) {
            LOG_ERROR(KERNEL, "Invalid thread id or buffer argument!");
            return epoc::error_argument;
        }

        kernel::process *process_to_operate = thr_to_operate->owning_process();

        if (is_write) {
            if (len > static_cast<std::int32_t>(buf->get_length())) {
                return epoc::error_overflow;
            }
        } else {
            if (len > static_cast<std::int32_t>(buf->get_max_length(process_to_operate))) {
                return epoc::error_underflow;
            }
        }

        std::uint8_t *buf_ptr = reinterpret_cast<std::uint8_t *>(buf->get_pointer_raw(crr));

        if (len == 4) {
            if (is_write) {
                std::uint32_t data = *reinterpret_cast<std::uint32_t *>(buf_ptr);

                if (!crr->write_dword_data_to(process_to_operate, addr, data)) {
                    return epoc::error_general;
                }
            } else {
                std::optional<std::uint32_t> result = crr->read_dword_data_from(process_to_operate, addr);
                if (!result.has_value()) {
                    return epoc::error_general;
                }

                std::uint32_t final_result_data = result.value();
                std::memcpy(buf_ptr, &final_result_data, 4);
            }

            return epoc::error_none;
        }

        std::uint8_t *dest_of_operate = reinterpret_cast<std::uint8_t *>(process_to_operate->get_ptr_on_addr_space(addr));

        if (!dest_of_operate) {
            LOG_WARN(KERNEL, "Destination to operate is null, return success still.");
            return epoc::error_none;
        }

        if (is_write) {
            std::memcpy(dest_of_operate, buf_ptr, len);
        } else {
            std::memcpy(buf_ptr, dest_of_operate, len);
        }

        codeseg_ptr ss = get_codeseg_from_addr(kern, process_to_operate, addr, false);

        if (ss) {
            kern->get_cpu()->imb_range(addr, len);
        }

        return epoc::error_none;
    }

    std::int32_t debug_command_do_print(kernel_system *kern, const address arg0) {
        kernel::process *crr = kern->crr_process();
        epoc::desc16 *buf = eka2l1::ptr<epoc::desc16>(arg0).get(crr);

        if (!buf) {
            return epoc::error_argument;
        }

        LOG_TRACE(EMULATED_STDOUT, "{}", common::ucs2_to_utf8(buf->to_std_string(crr)));
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, debug_command_execute, debug_cmd_header *header, const address arg0,
        const address arg1, const address arg2) {
        if (!header) {
            return epoc::error_argument;
        }

        kernel::process *crr = kern->crr_process();

        switch (header->opcode_) {
        case debug_cmd_opcode_read:
        case debug_cmd_opcode_write:
            return debug_command_do_read_write(kern, header->thread_id_, arg0, arg1,
                static_cast<std::int32_t>(arg2), (header->opcode_ == debug_cmd_opcode_write) ? true : false);

        case debug_cmd_opcode_print:
            return debug_command_do_print(kern, arg0);

        default: {
            LOG_ERROR(KERNEL, "Unimplemented debug command opcode {}", header->opcode_);
            break;
        }
        }

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, btrace_out, const std::uint32_t a0, const std::uint32_t a1, const std::uint32_t a2,
        const std::uint32_t a3) {
        config::state *conf = kern->get_config();

        if (!conf->enable_btrace) {
            return 1;
        }

        return static_cast<std::int32_t>(kern->get_btrace()->out(a0, a1, a2, a3));
    }

    BRIDGE_FUNC(std::int32_t, plat_sec_diagnostic, kernel::plat_sec_diagnostic *diag) {
        return epoc::error_permission_denied;
    }

    BRIDGE_FUNC(eka2l1::ptr<void>, get_global_userdata) {
        return 0;
    }

    BRIDGE_FUNC(address, exception_descriptor, address in_addr) {
        return epoc::get_exception_descriptor_addr(kern, in_addr);
    }

    BRIDGE_FUNC(address, exception_handler, kernel::handle h) {
        kernel::thread *thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return 0;
        }

        return thr->get_exception_handler();
    }

    BRIDGE_FUNC(std::int32_t, set_exception_handler, kernel::handle h, address handler, std::uint32_t mask) {
        kernel::thread *thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        thr->set_exception_handler(handler);
        thr->set_exception_mask(mask);

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, raise_exception, kernel::handle h, std::int32_t type) {
        LOG_ERROR(KERNEL, "Exception with type {} is thrown", type);
        kernel::thread *thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        if (thr == kern->crr_thread()) {
            return epoc::error_general;
        }

        return epoc::error_not_supported;
    }

    BRIDGE_FUNC(std::int32_t, is_exception_handled, kernel::handle h, std::int32_t type, bool aSwExcInProgress) {
        kernel::thread *thr = kern->get<kernel::thread>(h);

        if (!thr) {
            return epoc::error_bad_handle;
        }

        if (thr->get_exception_handler() == 0) {
            return 0;
        }

        return 1;
    }

    BRIDGE_FUNC(std::int32_t, safe_inc_32, eka2l1::ptr<std::int32_t> val_ptr) {
        std::int32_t *val = val_ptr.get(kern->crr_process());
        std::int32_t org_val = *val;
        *val > 0 ? val++ : 0;

        return org_val;
    }

    BRIDGE_FUNC(std::int32_t, safe_dec_32, eka2l1::ptr<std::int32_t> val_ptr) {
        std::int32_t *val = val_ptr.get(kern->crr_process());
        std::int32_t org_val = *val;
        *val > 0 ? val-- : 0;

        return org_val;
    }

    BRIDGE_FUNC(std::int32_t, locked_dec_32, eka2l1::ptr<std::int32_t> val_ptr) {
        std::int32_t *val = val_ptr.get(kern->crr_process());
        if (!val)
            return 0; // Prevent crash if null

        const std::int32_t before = *val;
        (*val)--;

        return before;
    }

    BRIDGE_FUNC(std::int32_t, locked_inc_32, eka2l1::ptr<std::int32_t> val_ptr) {
        std::int32_t *val = val_ptr.get(kern->crr_process());
        if (!val)
            return 0; // Prevent crash if null

        const std::int32_t before = *val;
        (*val)++;

        return before;
    }

    BRIDGE_FUNC(void, hle_dispatch, const std::uint32_t ordinal) {
        dispatcher_do_resolve(kern->get_system(), ordinal);
    }

    BRIDGE_FUNC(void, hle_dispatch_2) {
        const std::uint32_t *ordinal = eka2l1::ptr<std::uint32_t>(kern->get_cpu()->get_pc() + 4).get(kern->crr_process());
        dispatcher_do_resolve(kern->get_system(), *ordinal);
    }

    BRIDGE_FUNC(void, virtual_reality) {
        typedef bool (*reality_func)(void *data);

        const std::uint32_t current = kern->get_cpu()->get_pc();
        std::uint64_t *data = reinterpret_cast<std::uint64_t *>(kern->crr_process()->get_ptr_on_addr_space(current - 20));

        kernel::thread *thr = kern->crr_thread();
        kern->get_cpu()->save_context(thr->get_thread_context());

        reality_func to_call = reinterpret_cast<reality_func>(*data++);
        void *userdata = reinterpret_cast<void *>(*data++);

        if (!to_call(userdata)) {
            kern->crr_thread()->wait_for_any_request();
        }
    }

    BRIDGE_FUNC(std::uint32_t, math_rand) {
        return eka2l1::random();
    }

    BRIDGE_FUNC(void, add_event, epoc::raw_event *evt) {
        if (!evt) {
            LOG_ERROR(KERNEL, "Event to add is null, ignored");
            return;
        }

        dispatcher_do_event_add(kern->get_system(), *evt);
    }

    BRIDGE_FUNC(std::uint32_t, uchar_get_category, const epoc::uchar character) {
        return epoc::get_uchar_category(character, *kern->get_current_locale());
    }

    BRIDGE_FUNC(std::uint32_t, uchar_lowercase, const epoc::uchar character) {
        return epoc::lowercase_uchar(character, *kern->get_current_locale());
    }

    BRIDGE_FUNC(std::uint32_t, uchar_uppercase, const epoc::uchar character) {
        return epoc::uppercase_uchar(character, *kern->get_current_locale());
    }

    BRIDGE_FUNC(std::uint32_t, uchar_fold, const epoc::uchar character) {
        return epoc::fold_uchar(character, *kern->get_current_locale());
    }

    BRIDGE_FUNC(address, get_locale_char_set) {
        return kern->get_global_user_data_pointer().ptr_address() + offsetof(kernel_global_data, char_set_);
    }

    template <typename T>
    std::int32_t des_locate_fold(kernel_system *kern, epoc::desc<T> *des, const epoc::uchar character) {
        if (des->get_length() == 0) {
            return epoc::error_not_found;
        }

        const T *str_data = reinterpret_cast<const T *>(des->get_pointer(kern->crr_process()));
        std::locale &current_locale = *kern->get_current_locale();

        if (!str_data) {
            return epoc::error_bad_descriptor;
        }

        for (std::uint32_t i = 0; i < des->get_length(); i++) {
            if (fold_uchar(str_data[i], current_locale) == fold_uchar(character, current_locale)) {
                return static_cast<std::int32_t>(i);
            }
        }

        return epoc::error_not_found;
    }

    BRIDGE_FUNC(std::int32_t, des8_locate_fold, epoc::desc8 *des, const epoc::uchar character) {
        return des_locate_fold<char>(kern, des, character);
    }

    BRIDGE_FUNC(std::int32_t, des16_locate_fold, epoc::desc16 *des, const epoc::uchar character) {
        return des_locate_fold<char16_t>(kern, des, character);
    }

    BRIDGE_FUNC(std::int32_t, des8_match, epoc::desc8 *str_des, epoc::desc8 *seq_des, const std::int32_t is_fold) {
        kernel::process *crr_process = kern->crr_process();

        std::string source = str_des->to_std_string(crr_process);
        std::string sequence_search = seq_des->to_std_string(crr_process);

        const std::size_t pos = common::wildcard_match(source, sequence_search, is_fold);

        if (pos == std::wstring::npos) {
            return epoc::error_not_found;
        }

        return static_cast<std::int32_t>(pos);
    }

    BRIDGE_FUNC(std::int32_t, des16_match, epoc::desc16 *str_des, epoc::desc16 *seq_des, const std::int32_t is_fold) {
        kernel::process *crr_process = kern->crr_process();

        std::u16string source = str_des->to_std_string(crr_process);
        std::u16string sequence_search = seq_des->to_std_string(crr_process);

        const std::size_t pos = common::wildcard_match(common::ucs2_to_wstr(source), common::ucs2_to_wstr(sequence_search),
            is_fold);

        if (pos == std::wstring::npos) {
            return epoc::error_not_found;
        }

        return static_cast<std::int32_t>(pos);
    }

    template <typename T, typename F>
    std::int32_t desc_find(kernel_system *kern, F cvt_func, epoc::desc<T> *des, const T *str, const std::int32_t length, const bool is_fold) {
        if (length < 0) {
            return epoc::error_argument;
        }

        if (length == 0) {
            return epoc::error_not_found;
        }

        kernel::process *crr_pr = kern->crr_process();

        std::basic_string<T> source_str = des->to_std_string(crr_pr);
        std::basic_string<T> to_find_str(str, length);

        to_find_str = std::basic_string<T>(1, static_cast<T>('(')) + to_find_str;
        to_find_str += std::basic_string<T>(1, static_cast<T>(')'));

        const std::size_t pos = common::wildcard_match(cvt_func(source_str), cvt_func(to_find_str), is_fold);

        if (pos == std::basic_string<T>::npos) {
            return epoc::error_not_found;
        }

        return static_cast<std::int32_t>(pos);
    }

    inline std::string rereturn_string_function(std::string value) {
        return value;
    }

    BRIDGE_FUNC(std::int32_t, desc8_find, epoc::desc8 *dd, const char *str, const std::int32_t length, const bool is_fold) {
        return desc_find(kern, rereturn_string_function, dd, str, length, is_fold);
    }

    BRIDGE_FUNC(std::int32_t, desc16_find, epoc::desc16 *dd, const char16_t *str, const std::int32_t length, const bool is_fold) {
        return desc_find(kern, common::ucs2_to_wstr, dd, str, length, is_fold);
    }

    BRIDGE_FUNC(std::uint32_t, user_language) {
        return static_cast<std::uint32_t>(kern->get_current_language());
    }

    BRIDGE_FUNC(void, locale_refresh, epoc::locale *loc) {
        property_ptr prop = kern->get_prop(epoc::SYS_CATEGORY, epoc::LOCALE_DATA_KEY);
        if (!prop) {
            LOG_ERROR(KERNEL, "Locale property not available (not found)!");
            return;
        }

        std::optional<epoc::locale> loc_pkg = prop->get_pkg<epoc::locale>();
        if (!loc_pkg) {
            LOG_ERROR(KERNEL, "Locale property not available (data get failed)!");
            return;
        }

        std::memcpy(loc, &(loc_pkg.value()), sizeof(epoc::locale));
    }

    BRIDGE_FUNC(std::int32_t, dll_global_data_allocated, const address handle) {
        return (kern->get_global_dll_space(handle, nullptr) != 0);
    }

    BRIDGE_FUNC(std::int32_t, dll_global_data_write, const address handle, const std::int32_t pos,
        epoc::desc8 *data_to_write) {
        if (pos < 0) {
            return epoc::error_argument;
        }

        std::uint8_t *data_ptr = nullptr;
        std::uint32_t data_space_size = 0;

        if (kern->get_global_dll_space(handle, &data_ptr, &data_space_size) == 0) {
            return epoc::error_not_ready;
        }

        const std::uint32_t len_to_write = data_to_write->get_length();
        if (pos + len_to_write > data_space_size) {
            return epoc::error_no_memory;
        }

        char *data_to_write_ptr = data_to_write->get_pointer(kern->crr_process());
        if (!data_to_write_ptr) {
            return epoc::error_bad_descriptor;
        }

        std::memcpy(data_ptr + pos, data_to_write_ptr, len_to_write);
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, dll_global_data_read, const address handle, const std::int32_t pos,
        const std::int32_t size, epoc::des8 *place_to_read_to) {
        if ((pos < 0) || (size < 0)) {
            return epoc::error_argument;
        }

        std::uint8_t *data_ptr = nullptr;
        std::uint32_t data_space_size = 0;

        if (kern->get_global_dll_space(handle, &data_ptr, &data_space_size) == 0) {
            return epoc::error_not_ready;
        }

        const std::uint32_t len_to_read = common::min<std::int32_t>(static_cast<std::uint32_t>(size),
            common::min<std::uint32_t>(place_to_read_to->get_max_length(kern->crr_process()), data_space_size - pos));

        char *data_to_write_ptr = place_to_read_to->get_pointer(kern->crr_process());
        if (!data_to_write_ptr) {
            return epoc::error_bad_descriptor;
        }

        std::memcpy(data_to_write_ptr, data_ptr + pos, len_to_read);
        place_to_read_to->set_length(kern->crr_process(), len_to_read);

        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, bus_dev_open_socket, const std::uint32_t unkpadding, epoc::desc16 *ldd_name,
        epoc::desc16 *pdd_name) {
        if (ldd_name) {
            kernel::process *own_pr = kern->crr_process();

            const std::u16string ldd_name_str = ldd_name->to_std_string(own_pr);
            std::u16string pdd_name_str;

            if (pdd_name)
                pdd_name_str = pdd_name->to_std_string(own_pr);

            LOG_TRACE(KERNEL, "Ldd name: {}, Pdd name: {}", common::ucs2_to_utf8(ldd_name_str),
                common::ucs2_to_utf8(pdd_name_str));
        }

        LOG_TRACE(KERNEL, "Busdev socket opening stubbed with number 0");
        return epoc::error_none;
    }

    BRIDGE_FUNC(std::int32_t, logical_channel_do_control, const kernel::handle h, const int func,
        eka2l1::ptr<void> arg1, eka2l1::ptr<void> arg2) {
        ldd::channel *chn = kern->get<ldd::channel>(h);

        if (!chn) {
            return epoc::error_not_found;
        }

        return chn->do_control(kern->crr_thread(), func, arg1, arg2);
    }

    BRIDGE_FUNC(std::int32_t, logical_channel_do_request, const kernel::handle h, const int func,
        eka2l1::ptr<epoc::request_status> status, const std::uint32_t *args) {
        ldd::channel *chn = kern->get<ldd::channel>(h);

        if (!chn) {
            return epoc::error_not_found;
        }

        epoc::notify_info request_nof_info(status, kern->crr_thread());
        return chn->do_request(request_nof_info, func, args[0], args[1], false);
    }

    BRIDGE_FUNC(void, restore_thread_exception_state) {
        kern->crr_thread()->restore_before_exception_state();
    }

    BRIDGE_FUNC(std::uint32_t, superpage_config) {
        return 0;
    }

    BRIDGE_FUNC(std::int32_t, get_locale_dll_name) {
        return epoc::error_none;
    }

    const eka2l1::hle::func_map svc_register_funcs_v10 = {};
    const eka2l1::hle::func_map svc_register_funcs_v94 = {};

    const eka2l1::hle::func_map svc_register_funcs_v93 = {
        BRIDGE_REGISTER(0x00800000, wait_for_any_request),
        BRIDGE_REGISTER(0x00800001, heap),
        BRIDGE_REGISTER(0x00800002, heap_switch),
        BRIDGE_REGISTER(0x00800005, active_scheduler),
        BRIDGE_REGISTER(0x00800006, set_active_scheduler),
        BRIDGE_REGISTER(0x00800008, trap_handler),
        BRIDGE_REGISTER(0x00800009, set_trap_handler),
        BRIDGE_REGISTER(0x0080000D, debug_mask),
        BRIDGE_REGISTER(0x0080000F, fast_counter),
        BRIDGE_REGISTER(0x00800010, ntick_count),
        BRIDGE_REGISTER(0x00800013, user_svr_rom_header_address),
        BRIDGE_REGISTER(0x00800014, user_svr_rom_root_dir_address),
        BRIDGE_REGISTER(0x00800015, safe_inc_32),
        BRIDGE_REGISTER(0x00800016, safe_dec_32),
        BRIDGE_REGISTER(0x00800017, locked_inc_32),
        BRIDGE_REGISTER(0x00800018, locked_dec_32),
        BRIDGE_REGISTER(0x00800019, utc_offset),
        BRIDGE_REGISTER(0x0080001A, get_global_userdata),
        BRIDGE_REGISTER(0x00C10000, hle_dispatch),
        BRIDGE_REGISTER(0x00C10001, hle_dispatch_2),

        BRIDGE_REGISTER(0x00, object_next),
        BRIDGE_REGISTER(0x01, chunk_base),
        BRIDGE_REGISTER(0x02, chunk_size),
        BRIDGE_REGISTER(0x03, chunk_max_size),
        BRIDGE_REGISTER(0x05, tick_count),
        BRIDGE_REGISTER(0x0B, math_rand),
        BRIDGE_REGISTER(0x0C, imb_range),
        BRIDGE_REGISTER(0x0E, library_lookup),
        BRIDGE_REGISTER(0x0F, library_filename),
        BRIDGE_REGISTER(0x11, mutex_wait),
        BRIDGE_REGISTER(0x12, mutex_signal),
        BRIDGE_REGISTER(0x13, process_get_id),
        BRIDGE_REGISTER(0x14, dll_filename),
        BRIDGE_REGISTER(0x15, process_resume),
        BRIDGE_REGISTER(0x16, process_filename),
        BRIDGE_REGISTER(0x17, process_command_line),
        BRIDGE_REGISTER(0x18, process_exit_type),
        BRIDGE_REGISTER(0x19, process_exit_reason),
        BRIDGE_REGISTER(0x1A, process_exit_category),
        BRIDGE_REGISTER(0x1B, process_priority),
        BRIDGE_REGISTER(0x1C, process_set_priority),
        BRIDGE_REGISTER(0x1E, process_set_flags),
        BRIDGE_REGISTER(0x1F, semaphore_wait),
        BRIDGE_REGISTER(0x20, semaphore_signal),
        BRIDGE_REGISTER(0x21, semaphore_signal_n),
        BRIDGE_REGISTER(0x22, server_receive),
        BRIDGE_REGISTER(0x23, server_cancel),
        BRIDGE_REGISTER(0x24, set_session_ptr),
        BRIDGE_REGISTER(0x25, session_send),
        BRIDGE_REGISTER(0x26, thread_id),
        BRIDGE_REGISTER(0x27, session_share),
        BRIDGE_REGISTER(0x28, thread_resume),
        BRIDGE_REGISTER(0x29, thread_suspend),
        BRIDGE_REGISTER(0x2A, thread_priority),
        BRIDGE_REGISTER(0x2B, thread_set_priority),
        BRIDGE_REGISTER(0x2F, thread_set_flags),
        BRIDGE_REGISTER(0x30, thread_request_count),
        BRIDGE_REGISTER(0x31, thread_exit_type),
        BRIDGE_REGISTER(0x32, thread_exit_reason),
        BRIDGE_REGISTER(0x35, timer_cancel),
        BRIDGE_REGISTER(0x36, timer_after),
        BRIDGE_REGISTER(0x37, timer_at_utc),
        BRIDGE_REGISTER(0x39, change_notifier_logon),
        BRIDGE_REGISTER(0x3A, change_notifier_logoff),
        BRIDGE_REGISTER(0x3B, request_signal),
        BRIDGE_REGISTER(0x3C, handle_name),
        BRIDGE_REGISTER(0x3D, handle_full_name),
        BRIDGE_REGISTER(0x3E, handle_info),
        BRIDGE_REGISTER(0x3F, handle_count),
        BRIDGE_REGISTER(0x40, after),
        BRIDGE_REGISTER(0x42, message_complete),
        BRIDGE_REGISTER(0x43, message_complete_handle),
        BRIDGE_REGISTER(0x44, time_now),
        BRIDGE_REGISTER(0x45, set_utc_time_and_offset),

        BRIDGE_REGISTER(0x4B, add_event),
        BRIDGE_REGISTER(0x4C, session_send_sync),
        BRIDGE_REGISTER(0x4D, dll_tls),
        BRIDGE_REGISTER(0x4E, hal_function),
        BRIDGE_REGISTER(0x51, process_command_line_length),
        BRIDGE_REGISTER(0x54, clear_inactivity_time),
        BRIDGE_REGISTER(0x55, debug_print),
        BRIDGE_REGISTER(0x59, exception_handler),
        BRIDGE_REGISTER(0x5A, set_exception_handler),
        BRIDGE_REGISTER(0x5D, is_exception_handled),
        BRIDGE_REGISTER(0x5E, process_get_memory_info),
        BRIDGE_REGISTER(0x5F, library_get_memory_info),
        BRIDGE_REGISTER(0x63, process_type),
        BRIDGE_REGISTER(0x65, chunk_top),
        BRIDGE_REGISTER(0x67, thread_create),
        BRIDGE_REGISTER(0x68, handle_open_object_by_find_handle),
        BRIDGE_REGISTER(0x69, handle_close),
        BRIDGE_REGISTER(0x6A, chunk_new),
        BRIDGE_REGISTER(0x6B, chunk_adjust),
        BRIDGE_REGISTER(0x6C, handle_open_object),
        BRIDGE_REGISTER(0x6D, handle_duplicate),
        BRIDGE_REGISTER(0x6E, mutex_create),
        BRIDGE_REGISTER(0x6F, semaphore_create),
        BRIDGE_REGISTER(0x70, thread_open_by_id),
        BRIDGE_REGISTER(0x72, thread_kill),
        BRIDGE_REGISTER(0x73, thread_logon),
        BRIDGE_REGISTER(0x74, thread_logon_cancel),
        BRIDGE_REGISTER(0x75, dll_set_tls),
        BRIDGE_REGISTER(0x76, dll_free_tls),
        BRIDGE_REGISTER(0x77, thread_rename),
        BRIDGE_REGISTER(0x78, process_rename),
        BRIDGE_REGISTER(0x79, process_kill),
        BRIDGE_REGISTER(0x7A, process_logon),
        BRIDGE_REGISTER(0x7B, process_logon_cancel),
        BRIDGE_REGISTER(0x7C, thread_process),
        BRIDGE_REGISTER(0x7D, server_create),
        BRIDGE_REGISTER(0x7E, session_create),
        BRIDGE_REGISTER(0x7F, session_create_from_handle),
        BRIDGE_REGISTER(0x83, timer_create),
        BRIDGE_REGISTER(0x84, timer_after),
        BRIDGE_REGISTER(0x85, after),
        BRIDGE_REGISTER(0x86, change_notifier_create),
        BRIDGE_REGISTER(0x9B, wait_dll_lock),
        BRIDGE_REGISTER(0x9C, release_dll_lock),
        BRIDGE_REGISTER(0x9D, library_attach),
        BRIDGE_REGISTER(0x9E, library_attached),
        BRIDGE_REGISTER(0x9F, static_call_list),
        BRIDGE_REGISTER(0xA0, library_detach),
        BRIDGE_REGISTER(0xA1, library_detached),
        BRIDGE_REGISTER(0xA2, last_thread_handle),
        BRIDGE_REGISTER(0xA3, thread_rendezvous),
        BRIDGE_REGISTER(0xA4, process_rendezvous),
        BRIDGE_REGISTER(0xA5, message_get_des_length),
        BRIDGE_REGISTER(0xA6, message_get_des_max_length),
        BRIDGE_REGISTER(0xA7, message_ipc_copy),
        BRIDGE_REGISTER(0xA8, message_client),
        BRIDGE_REGISTER(0xAA, message_construct),
        BRIDGE_REGISTER(0xAB, message_kill),
        BRIDGE_REGISTER(0xAC, message_open_handle),
        BRIDGE_REGISTER(0xAD, process_security_info),
        BRIDGE_REGISTER(0xAE, thread_security_info),
        BRIDGE_REGISTER(0xAF, message_security_info),
        BRIDGE_REGISTER(0xB0, creator_security_info),
        BRIDGE_REGISTER(0xB3, message_queue_create),
        BRIDGE_REGISTER(0xB4, message_queue_send),
        BRIDGE_REGISTER(0xB5, message_queue_receive),
        BRIDGE_REGISTER(0xB8, message_queue_notify_data_available),
        BRIDGE_REGISTER(0xB9, message_queue_cancel_notify_available),
        BRIDGE_REGISTER(0xBB, property_define),
        BRIDGE_REGISTER(0xBC, property_delete),
        BRIDGE_REGISTER(0xBD, property_attach),
        BRIDGE_REGISTER(0xBE, property_subscribe),
        BRIDGE_REGISTER(0xBF, property_cancel),
        BRIDGE_REGISTER(0xC0, property_get_int),
        BRIDGE_REGISTER(0xC1, property_get_bin),
        BRIDGE_REGISTER(0xC3, property_set_bin),
        BRIDGE_REGISTER(0xC4, property_find_get_int),
        BRIDGE_REGISTER(0xC5, property_find_get_bin),
        BRIDGE_REGISTER(0xC6, property_find_set_int),
        BRIDGE_REGISTER(0xC7, property_find_set_bin),
        BRIDGE_REGISTER(0xCD, process_set_handle_parameter),
        BRIDGE_REGISTER(0xCE, process_set_data_parameter),
        BRIDGE_REGISTER(0xCF, process_get_handle_parameter),
        BRIDGE_REGISTER(0xD0, process_get_data_parameter),
        BRIDGE_REGISTER(0xD1, process_data_parameter_length),
        BRIDGE_REGISTER(0xD3, thread_stack_info),
        BRIDGE_REGISTER(0xDA, plat_sec_diagnostic),
        BRIDGE_REGISTER(0xDB, exception_descriptor),
        BRIDGE_REGISTER(0xDC, thread_request_signal),
        BRIDGE_REGISTER(0xDD, mutex_is_held),
        BRIDGE_REGISTER(0xDE, leave_start),
        BRIDGE_REGISTER(0xDF, leave_end),
        BRIDGE_REGISTER(0xE4, session_security_info),
        BRIDGE_REGISTER(0xE7, btrace_out)
    };

    const eka2l1::hle::func_map svc_register_funcs_v80 = {};
    const eka2l1::hle::func_map svc_register_funcs_v81a = {};
    const eka2l1::hle::func_map svc_register_funcs_v6 = {};
}