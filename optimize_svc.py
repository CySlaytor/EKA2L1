import re
import os
from pathlib import Path

# Unused system calls exclusive to legacy / Belle architectures (not in v93)
FUNCTIONS_TO_REMOVE = [
    # EKA1 Process & Handle Helpers
    "process_type_eka1",
    "process_set_type_eka1",
    "process_set_priority_eka1",
    "process_set_flags_eka1",
    "handle_info_eka1",
    "handle_name_eka1",
    "process_command_line_eka1",
    "process_filename_eka1",
    "duplicate_handle_eka1",
    "open_object_eka1",
    "open_object_through_find_handle_eka1",
    "close_handle_eka1",
    "undertaker_create_eka1",
    "undertaker_logon_eka1",

    # EKA1 Sync Objects
    "semaphore_wait_eka1",
    "semaphore_count_eka1",
    "semaphore_signal_eka1",
    "semaphore_signal_n_eka1",
    "mutex_wait_eka1",
    "mutex_signal_eka1",
    "mutex_count_eka1",
    "thread_set_flags_eka1",

    # EKA1 Timers & Descriptors
    "timer_after_eka1",
    "timer_after_ticks_eka1",
    "timer_at_eka1",
    "thread_read_ipc_to_des8",
    "thread_read_ipc_to_des16",
    "thread_write_ipc_to_des8",
    "thread_write_ipc_to_des16",
    "thread_ipc_to_des_eka1",
    "thread_get_des_length",
    "thread_get_des_max_length",
    "thread_get_des_length_general",
    "thread_request_complete_eka1",
    "message_complete_eka1",
    "thread_set_priority_eka1",
    "message_ipc_copy_eka1",

    # EKA1 Navigation & Subsystem Scanning
    "process_find_next",
    "server_find_next",
    "thread_find_next",
    "object_next_eka1",
    "user_svr_hal_get",
    "user_svr_screen_info",
    "user_svr_dll_filename",
    "library_lookup_eka1",
    "process_filename_eka1",
    "library_filename_eka1",
    "library_type_eka1",
    "logical_channel_do_control_eka1",
    "logical_channel_do_request_eka1",

    # EKA1 Core Executor Sub-methods
    "the_executor_eka1",
    "chunk_create_eka1",
    "mutex_create_eka1",
    "sema_create_eka1",
    "sema_open_eka1",
    "thread_create_eka1",
    "session_create_eka1",
    "session_share_eka1",
    "thread_logon_eka1",
    "thread_logon_cancel_eka1",
    "duplicate_handle_eka1",
    "dll_set_tls_eka1",
    "dll_free_tls_eka1",
    "chunk_adjust_eka1",
    "chunk_adjust_double_ended_eka1",
    "thread_panic_eka1",
    "thread_rename_eka1",
    "thread_kill_eka1",
    "process_rename_eka1",
    "process_logon_eka1",
    "process_logon_cancel_eka1",
    "process_rendezvous_eka1",
    "process_rendezvous_complete_eka1",
    "process_open_by_id_eka1",
    "thread_open_eka1",
    "thread_open_by_id_eka1",
    "thread_get_heap_eka1",
    "thread_set_initial_parameter_eka1",
    "thread_get_own_process_eka1",
    "thread_rendezvous_eka1",
    "thread_rendezvous_complete_eka1",
    "server_create_eka1",
    "timer_create_eka1",
    "debug_open_eka1",
    "debug_close_eka1",
    "compress_heap_eka1",
    "dll_global_data_alloc",
    "add_logical_device_eka1",
    "create_logical_channel_eka1",
    "free_logical_device_eka1",
    "physical_device_add_eka1",
    "message2_kill_sender_eka1",
    "message2_client_eka1",
    "property_define_eka1",
    "property_attach_eka1",
    "msgqueue_create_eka1",
    "msgqueue_send_eka1",
    "msgqueue_receive_eka1",
    "msgqueue_cancel_data_available_eka1",
    "change_notifier_create_eka1",
    "session_send_sync_eka1",
    "session_send_eka1",

    # Exception & Thread Context stubs
    "dll_tls_eka1",
    "is_exception_handled_eka1",
    "raise_exception_eka1",
    "set_exception_handler_eka1"
]

UNUSED_MAPS = [
    "svc_register_funcs_v10",
    "svc_register_funcs_v94",
    "svc_register_funcs_v80",
    "svc_register_funcs_v81a",
    "svc_register_funcs_v6"
]


def find_matching_brace(content, start_idx):
    """
    Scans forward through C++ source code from start_idx (position of open '{'),
    counting braces while properly ignoring strings, character literals,
    and single/multi-line comments.
    """
    i = start_idx
    brace_count = 0
    in_string = False
    in_char = False
    in_line_comment = False
    in_block_comment = False
    
    while i < len(content):
        char = content[i]
        
        # 1. Handle block comments /* ... */
        if in_block_comment:
            if char == '/' and content[i-1] == '*':
                in_block_comment = False
            i += 1
            continue
            
        # 2. Handle single-line comments // ... \n
        if in_line_comment:
            if char == '\n':
                in_line_comment = False
            i += 1
            continue
            
        # 3. Handle string literal escapes and ending \"
        if in_string:
            if char == '\\':
                i += 2  # Skip escaped character
                continue
            if char == '"':
                in_string = False
            i += 1
            continue
            
        # 4. Handle character literal escapes
        if in_char:
            if char == '\\':
                i += 2
                continue
            if char == "'":
                in_char = False
            i += 1
            continue
            
        # Check transitions into comments / literals
        if char == '/' and i + 1 < len(content) and content[i+1] == '*':
            in_block_comment = True
            i += 2
            continue
        if char == '/' and i + 1 < len(content) and content[i+1] == '/':
            in_line_comment = True
            i += 2
            continue
        if char == '"':
            in_string = True
            i += 1
            continue
        if char == "'":
            in_char = True
            i += 1
            continue
            
        # Track active code braces
        if char == '{':
            brace_count += 1
        elif char == '}':
            brace_count -= 1
            if brace_count == 0:
                return i + 1  # Index directly after matching '}'
        i += 1
        
    return -1


def remove_cpp_block(content, target_name):
    # Match standard BRIDGE_FUNC declarations or direct function definitions
    pattern = r"(?:BRIDGE_FUNC\([^,]+,\s*" + re.escape(target_name) + r"\b|\b" + re.escape(target_name) + r"\b\s*\()[^{]*?{"
    match = re.search(pattern, content)
    if not match:
        return content, False
        
    start_idx = match.start()
    brace_start = match.end() - 1  # Index of '{'
    
    end_idx = find_matching_brace(content, brace_start)
    if end_idx != -1:
        content = content[:start_idx] + content[end_idx:].lstrip()
        return content, True
    return content, False


def empty_map_block(content, map_name):
    pattern = r"const eka2l1::hle::func_map\s+" + re.escape(map_name) + r"\s*=\s*\{"
    match = re.search(pattern, content)
    if not match:
        return content, False
        
    start_idx = match.start()
    
    brace_match = re.search(r"\{", content[start_idx:])
    if not brace_match:
        return content, False
        
    brace_start = start_idx + brace_match.start()
    
    end_idx = find_matching_brace(content, brace_start)
    if end_idx != -1:
        replacement = f"const eka2l1::hle::func_map {map_name} = {{}};\n\n"
        content = content[:start_idx] + replacement + content[end_idx:].lstrip()
        return content, True
    return content, False


def find_repository_root() -> Path:
    local_check = Path("src/emu/kernel/src/svc.cpp")
    if local_check.exists():
        return Path(".")

    default_absolute = Path(r"C:\Users\CySlaytor\source\repos\EKA2L1")
    if (default_absolute / "src/emu/kernel/src/svc.cpp").exists():
        return default_absolute

    print("[!] Repository not found automatically.")
    while True:
        user_input = input("Please enter or paste the path to your EKA2L1 root directory:\n> ").strip()
        clean_path = Path(user_input.replace('"', '').replace("'", ""))
        if (clean_path / "src/emu/kernel/src/svc.cpp").exists():
            return clean_path
        print("[!] That path does not appear to contain src/emu/kernel/src/svc.cpp. Try again.")


def optimize_svc():
    repo_root = find_repository_root()
    target_path = repo_root / "src/emu/kernel/src/svc.cpp"

    print(f"\n[*] Analyzing and optimizing: {target_path}")
    
    with open(target_path, "r", encoding="utf-8", errors="ignore") as f:
        content = f.read()

    orig_length = len(content.splitlines())
    print(f"    - Original line count: {orig_length}")

    # Step 1: Empty dispatch tables
    maps_emptied = 0
    for map_name in UNUSED_MAPS:
        content, success = empty_map_block(content, map_name)
        if success:
            maps_emptied += 1

    print(f"    - Emptied {maps_emptied} unused registration maps.")

    # Step 2: Delete associated HLE bridge function blocks
    funcs_removed = 0
    for func_name in FUNCTIONS_TO_REMOVE:
        while True:
            content, success = remove_cpp_block(content, func_name)
            if success:
                funcs_removed += 1
            else:
                break

    print(f"    - Safely sliced out {funcs_removed} redundant functions.")

    # Step 3: Write changes back
    new_length = len(content.splitlines())
    with open(target_path, "w", encoding="utf-8") as f:
        f.write(content)

    print(f"[*] Optimization finished.")
    print(f"    - Final line count: {new_length} lines.")
    print(f"    - Net reduction: {orig_length - new_length} lines ({((orig_length - new_length)/orig_length)*100:.2f}% size reduction).")


if __name__ == "__main__":
    optimize_svc()