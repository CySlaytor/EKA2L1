#include <common/configure.h>

#include <common/algorithm.h>
#include <common/chunkyseri.h>
#include <common/container.h>
#include <common/cvt.h>
#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>
#include <common/platform.h>
#include <common/random.h>

#include <disasm/disasm.h>
#include <system/consts.h>
#include <system/epoc.h>
#include <system/hal.h>
#include <utils/panic.h>

#ifdef ENABLE_SCRIPTING
#include <scripting/manager.h>
#endif

#include <services/applist/applist.h>

#include <atomic>
#include <fstream>
#include <string>

#include <drivers/itc.h>
#include <gdbstub/gdbstub.h>

#include <kernel/kernel.h>
#include <mem/mem.h>
#include <mem/ptr.h>

#include <dispatch/dispatcher.h>
#include <dispatch/libraries/register.h>
#include <kernel/libmanager.h>
#include <kernel/timing.h>
#include <loader/rom.h>
#include <services/init.h>
#include <vfs/vfs.h>

#include <cpu/arm_factory.h>
#include <cpu/arm_utils.h>

#include <config/app_settings.h>
#include <config/config.h>

#include <services/window/screen.h>
#include <services/window/window.h>
#include <system/devices.h>
#include <system/software.h>

namespace eka2l1 {
#define HAL_ENTRY(generic_name, display_name, num, num_old) hal_entry_##generic_name = num,

    enum hal_entry {
#include <kernel/hal.def>
    };

    static const char *PATCH_FOLDER_PATH = ".//patch//";

    system_create_components::system_create_components()
        : graphics_(nullptr)
        , audio_(nullptr)
        , conf_(nullptr)
        , settings_(nullptr) {
    }

    class system_impl {
        std::mutex mut;

        arm::core_instance cpu;
        arm::exclusive_monitor_instance exmonitor;

        arm_emulator_type cpu_type;

        drivers::graphics_driver *gdriver;
        drivers::audio_driver *adriver;
        drivers::sensor_driver *ssdriver;

        std::unique_ptr<memory_system> mem_;
        std::unique_ptr<kernel_system> kern_;
        std::unique_ptr<device_manager> dvcmngr_;
        std::unique_ptr<ntimer> timing_;
        std::unique_ptr<io_system> io_;
        std::unique_ptr<disasm> disassembler_;
        std::unique_ptr<gdbstub> stub_;
        std::unique_ptr<dispatch::dispatcher> dispatcher_;

#if ENABLE_SCRIPTING
        std::unique_ptr<manager::scripts> scripting_;
#endif

        debugger_base *debugger_;
        loader::rom romf_;
        window_server *winserv_;

        config::state *conf_;
        config::app_settings *app_settings_;

        std::atomic<bool> exit = false;
        std::atomic<bool> paused = false;

        std::unordered_map<uint32_t, hal_instance> hals_;

        bool startup_inited = false;

        std::optional<filesystem_id> rom_fs_id_;
        std::optional<filesystem_id> physical_fs_id_;

        system *parent_;

        std::size_t gdb_stub_breakpoint_callback_handle_;

        common::identity_container<system_reset_callback_type> reset_callbacks_;

    public:
        explicit system_impl(system *parent, system_create_components &param);

        ~system_impl() {
#if ENABLE_SCRIPTING
            scripting_.reset();
#endif
            if (dispatcher_)
                dispatcher_->shutdown(gdriver);
            dispatcher_.reset();
            if (kern_)
                kern_->wipeout();
            kern_.reset();
            mem_.reset();
            timing_.reset();
        };

        void set_graphics_driver(drivers::graphics_driver *graphics_driver);
        void set_audio_driver(drivers::audio_driver *audio_driver);
        void set_sensor_driver(drivers::sensor_driver *sensor_driver);
        void set_debugger(debugger_base *new_debugger) { debugger_ = new_debugger; }

        void setup_outsider() {
            service::init_services(parent_);
            set_system_language(static_cast<language>(conf_->language));
            epoc::init_hal(parent_);

            dispatcher_ = std::make_unique<dispatch::dispatcher>(kern_.get(), timing_.get());
            if (gdriver)
                dispatcher_->set_graphics_driver(gdriver);

            winserv_ = reinterpret_cast<window_server *>(kern_->get_by_name<service::server>(eka2l1::get_winserv_name_by_epocver(kern_->get_epoc_version())));

            if (!stub_->is_server_enabled() && conf_->enable_gdbstub) {
                stub_->init(kern_.get(), io_.get());
                stub_->toggle_server(true);
            }

            if (stub_->is_server_enabled()) {
                gdb_stub_breakpoint_callback_handle_ = kern_->register_breakpoint_hit_callback([this](arm::core *cpu_core, kernel::thread *target, const std::uint32_t addr) {
                    if (stub_->is_connected()) {
                        cpu_core->stop();
                        cpu_core->set_pc(addr);
                        cpu_core->save_context(target->get_thread_context());
                        stub_->break_exec();
                        stub_->send_trap_gdb(target, 5);
                    }
                });
            }
        }

        bool is_s80_device_active() { return false; } // Hardcoded out for OS 9.3 target

        void set_symbian_version_use(const epocver ever) {
            io_->set_epoc_ver(ever);
            mem_ = std::make_unique<memory_system>(exmonitor.get(), conf_, mem::mem_model_type::multiple, false);
            io_->install_memory(mem_.get());
            kern_->install_memory(mem_.get());
            kern_->set_epoc_version(ever);
            kern_->set_capped_cpu_hz(preset::SYSTEM_CPU_HZ_S60V3);
        }

        void start_access() {
            paused = true;
            if (kern_)
                kern_->stop_cores_idling();
            mut.lock();
        }

        void end_access() {
            paused = false;
            mut.unlock();
        }

        bool set_device(const std::uint8_t idx) {
            start_access();
            if (!reset(false, static_cast<std::int32_t>(idx))) {
                end_access();
                return false;
            }
            end_access();
            return true;
        }

        bool rescan_devices(const drive_number romdrv) {
            bool actually_found = false;
            {
                start_access();
                dvcmngr_->clear();
                std::string rom_drive_name = std::string(1, static_cast<char>(drive_to_char16(romdrv)));
                std::string storage_path;
                common::get_current_directory(storage_path);
                storage_path = eka2l1::absolute_path(conf_->storage, storage_path);

                std::string root_z_path = add_path(storage_path, "drives/" + rom_drive_name + "/");
                auto ite = common::make_directory_iterator(root_z_path, "");
                if (!ite)
                    return false;

                ite->detail = true;
                common::dir_entry firm_entry;

                while (ite->next_entry(firm_entry) == 0) {
                    if ((firm_entry.type == common::file_type::FILE_DIRECTORY) && (firm_entry.name != ".") && (firm_entry.name != "..")) {
                        const std::string full_entry_path = eka2l1::add_path(root_z_path, firm_entry.name);
                        const epocver ver = epocver::epoc93fp2; // Hardcode to 9.3
                        std::string manu, firm_name, model;
                        loader::determine_rpkg_product_info(full_entry_path, manu, firm_name, model);

                        const std::string rom_directory = eka2l1::add_path(storage_path, eka2l1::add_path("roms", firm_name + "\\"));
                        const std::string rom_file = eka2l1::add_path(rom_directory, "SYM.ROM");
                        if (!common::exists(rom_file))
                            continue;

                        if (dvcmngr_->add_new_device(firm_name, model, manu, ver, 0) == add_device_none) {
                            actually_found = true;
                        }
                    }
                }
                dvcmngr_->save_devices();
                end_access();
            }

            if (actually_found) {
                conf_->device = 0;
                conf_->serialize(false);
                set_device(0);
            }
            return true;
        }

        void validate_current_device() { io_->validate_for_host(); }
        std::size_t add_system_reset_callback(system_reset_callback_type type) { return reset_callbacks_.add(type); }
        bool remove_system_reset_callback(const std::size_t h) { return reset_callbacks_.remove(h); }
        void invoke_system_reset_callbacks() {
            for (auto cb : reset_callbacks_)
                cb(parent_);
        }

        void set_cpu_executor_type(const arm_emulator_type type) { cpu_type = type; }
        const arm_emulator_type get_cpu_executor_type() const { return cpu_type; }

        config::state *get_config() { return conf_; }
        device_manager *get_device_manager() { return dvcmngr_.get(); }
        manager::scripts *get_scripts() {
#if ENABLE_SCRIPTING
            return scripting_.get();
#else
            return nullptr;
#endif
        }
        void set_config(config::state *confs) { conf_ = confs; }
        loader::rom *get_rom_info() { return &romf_; }
        epocver get_symbian_version_use() const { return kern_->get_epoc_version(); }
        void prepare_reschedule() { cpu->stop(); }
        const language get_system_language() const { return kern_->get_current_language(); }
        void set_system_language(const language new_lang) { kern_->set_current_language(new_lang); }

        void startup();
        bool load(const std::u16string &path, const std::u16string &cmd_arg);
        int loop();
        bool pause();
        bool unpause();

        memory_system *get_memory_system() { return mem_.get(); }
        kernel_system *get_kernel_system() { return kern_.get(); }
        hle::lib_manager *get_lib_manager() { return kern_->get_lib_manager(); }
        io_system *get_io_system() { return io_.get(); }
        ntimer *get_ntimer() { return timing_.get(); }
        disasm *get_disasm() { return disassembler_.get(); }
        gdbstub *get_gdb_stub() { return stub_.get(); }
        drivers::graphics_driver *get_graphic_driver() { return gdriver; }
        drivers::audio_driver *get_audio_driver() { return adriver; }
        drivers::sensor_driver *get_sensor_driver() { return ssdriver; }
        arm::core *get_cpu() { return cpu.get(); }
        dispatch::dispatcher *get_dispatcher() { return dispatcher_.get(); }

        void mount(drive_number drv, const drive_media media, std::string path, const std::uint32_t attrib = io_attrib_none);

        bool reset(const bool lock_sys, const std::int32_t new_index = -1);
        void do_state(common::chunkyseri &seri);
        bool load_rom(const std::string &path);
        void request_exit();
        bool should_exit() const { return exit; }

        void add_new_hal(uint32_t hal_category, hal_instance &hal_com);
        epoc::hal *get_hal(uint32_t category);
        void initialize_user_parties();
    };

    void system_impl::do_state(common::chunkyseri &seri) {}

    void system_impl::startup() {
        exit = false;
        timing_ = std::make_unique<ntimer>(preset::SYSTEM_CPU_HZ_S60V3);
        timing_->set_realtime_level(get_realtime_level_from_string(conf_->rtos_level.c_str()));

        file_system_inst physical_fs = create_physical_filesystem(epocver::epoc93fp2, "");
        physical_fs_id_ = io_->add_filesystem(physical_fs);

        exmonitor = arm::create_exclusive_monitor(cpu_type, 1);
        cpu = arm::create_core(exmonitor.get(), cpu_type);

        kern_ = std::make_unique<kernel_system>(parent_, timing_.get(), io_.get(), conf_, app_settings_, &romf_, cpu.get(), disassembler_.get());
        epoc::init_panic_descriptions();
    }

    system_impl::system_impl(system *parent, system_create_components &param)
        : parent_(parent)
        , debugger_(nullptr)
        , gdriver(param.graphics_)
        , adriver(param.audio_)
        , conf_(param.conf_)
        , app_settings_(param.settings_)
        , exit(false) {
        cpu_type = arm_emulator_type::dynarmic; // Forced dynarmic for performance
        dvcmngr_ = std::make_unique<device_manager>(conf_);
        disassembler_ = std::make_unique<disasm>();
        io_ = std::make_unique<io_system>();
        stub_ = std::make_unique<gdbstub>();
    }

    void system_impl::set_graphics_driver(drivers::graphics_driver *graphics_driver) {
        start_access();
        gdriver = graphics_driver;
        if (dispatcher_)
            dispatcher_->set_graphics_driver(gdriver);
        end_access();
    }

    void system_impl::set_audio_driver(drivers::audio_driver *aud_driver) { adriver = aud_driver; }
    void system_impl::set_sensor_driver(drivers::sensor_driver *sensor_driver) { ssdriver = sensor_driver; }

    bool system_impl::load(const std::u16string &path, const std::u16string &cmd_arg) {
        process_ptr pr = kern_->spawn_new_process(path, cmd_arg);
        if (!pr)
            return false;
        pr->run();
        return true;
    }

    bool system_impl::pause() {
        paused = true;
        if (kern_)
            kern_->stop_cores_idling();
        const std::lock_guard<std::mutex> guard(mut);
        if (timing_)
            timing_->set_paused(true);
        return true;
    }

    bool system_impl::unpause() {
        paused = false;
        const std::lock_guard<std::mutex> guard(mut);
        if (timing_)
            timing_->set_paused(false);
        return true;
    }

    int system_impl::loop() {
        const std::lock_guard<std::mutex> guard(mut);
        if (paused)
            return 1;

        bool should_step = false;
        bool script_hits_the_feels = false;
        kernel::thread *to_run = kern_->crr_thread();

#ifdef ENABLE_SCRIPTING
        manager::scripts *scripter = get_scripts();
#endif

        if (stub_->is_server_enabled()) {
            stub_->handle_packet();
            if (stub_->get_cpu_halt_flag()) {
                if (stub_->get_cpu_step_flag())
                    should_step = true;
                else
                    return 1;
            }
        } else {
#ifdef ENABLE_SCRIPTING
            if (scripter->last_breakpoint_hit(to_run)) {
                script_hits_the_feels = true;
                should_step = true;
            }
#endif
            if (conf_->stepping && !should_step)
                should_step = true;
        }

        if (to_run != nullptr) {
            if (!should_step) {
                cpu->run(to_run->get_remaining_screenticks());
            } else {
                cpu->step();
#ifdef ENABLE_SCRIPTING
                if (script_hits_the_feels)
                    scripter->reset_breakpoint_hit(cpu.get(), to_run);
#endif
            }
            to_run->add_ticks(cpu->get_num_instruction_executed());
        }

        if (!kern_->should_terminate()) {
            kern_->reschedule();
        } else {
            exit = true;
            return 0;
        }
        return 1;
    }

    bool system_impl::load_rom(const std::string &path) {
        symfile f = eka2l1::physical_file_proxy(path, READ_MODE | BIN_MODE);
        if (!f || !f->valid()) {
            LOG_ERROR(SYSTEM, "ROM file not present: {}", path);
            return false;
        }

        eka2l1::ro_file_stream rom_fstream(f.get());
        std::optional<loader::rom> romf_res = loader::load_rom(reinterpret_cast<common::ro_stream *>(&rom_fstream));
        if (!romf_res)
            return false;

        rom_fstream.seek(0, common::seek_where::beg);
        romf_ = std::move(*romf_res);

        if (!rom_fs_id_.has_value()) {
            auto current_device = dvcmngr_->get_current();
            if (!current_device)
                return false;
            file_system_inst rom_fs = create_rom_filesystem(&romf_, mem_.get(), get_symbian_version_use(), current_device->firmware_code);
            rom_fs_id_ = io_->add_filesystem(rom_fs);
        }

        return kern_->map_rom(romf_.header.rom_base, path);
    }

    void system_impl::mount(drive_number drv, const drive_media media, std::string path, const std::uint32_t attrib) {
        io_->mount_physical_path(drv, media, attrib, common::utf8_to_ucs2(path));
    }

    void system_impl::initialize_user_parties() {
        get_lib_manager()->load_patch_libraries(PATCH_FOLDER_PATH);
        dispatch::libraries::register_functions(kern_.get(), dispatcher_.get());
        service::init_services_post_bootup(parent_);
#ifdef ENABLE_SCRIPTING
        scripting_->import_all_modules();
#endif
        kern_->start_bootload();
    }

    void system_impl::request_exit() {
        cpu->stop();
        exit = true;
    }

    bool system_impl::reset(const bool lock_sys, const std::int32_t index) {
        if (lock_sys)
            start_access();

        if ((index >= 0) && (static_cast<std::size_t>(index) >= dvcmngr_->total())) {
            if (lock_sys)
                end_access();
            return false;
        }

        exit = false;

#ifdef ENABLE_SCRIPTING
        if (scripting_)
            scripting_.reset();
#endif

        kern_->unregister_breakpoint_hit_callback(gdb_stub_breakpoint_callback_handle_);

        if (dispatcher_)
            dispatcher_->shutdown(gdriver);
        if (cpu)
            cpu->clear_instruction_cache();
        if (kern_)
            kern_->reset();
        if (timing_)
            timing_->reset();

        hals_.clear();

        if (index >= 0) {
            if (!dvcmngr_->set_current(static_cast<std::uint8_t>(index))) {
                if (lock_sys)
                    end_access();
                return false;
            }
        }

        device *dvc = dvcmngr_->get_current();

        const bool lang_not_found_in_device = (std::find(dvc->languages.begin(), dvc->languages.end(), conf_->language) == dvc->languages.end());
        if ((conf_->language == -1) || lang_not_found_in_device) {
            conf_->language = dvc->default_language_code;
            conf_->serialize(false);
        }

        io_->set_product_code(dvc->firmware_code);
        set_symbian_version_use(epocver::epoc93fp2); // Hardcoded OS 9.3 target

        cpu->clear_instruction_cache();

        const std::string rom_path = add_path(conf_->storage, add_path(preset::ROM_FOLDER_PATH, add_path(common::lowercase_string(dvc->firmware_code), preset::ROM_FILENAME)));
        load_rom(rom_path);

#ifdef ENABLE_SCRIPTING
        scripting_ = std::make_unique<manager::scripts>(parent_);
#endif

        if (gdriver)
            gdriver->set_upscale_shader("");

        setup_outsider();
        invoke_system_reset_callbacks();

        if (lock_sys)
            end_access();

        return true;
    }

    void system_impl::add_new_hal(uint32_t hal_category, hal_instance &hal_com) { hals_.emplace(hal_category, std::move(hal_com)); }
    epoc::hal *system_impl::get_hal(uint32_t category) { return hals_[category].get(); }

    // System delegates
    system::system(system_create_components &comp)
        : impl(new system_impl(this, comp)) {}
    system::~system() { delete impl; }
    config::state *system::get_config() { return impl->get_config(); }
    device_manager *system::get_device_manager() { return impl->get_device_manager(); }
    manager::scripts *system::get_scripts() { return impl->get_scripts(); }
    void system::set_config(config::state *conf) { impl->set_config(conf); }
    void system::set_graphics_driver(drivers::graphics_driver *driver) { impl->set_graphics_driver(driver); }
    void system::set_audio_driver(drivers::audio_driver *driver) { impl->set_audio_driver(driver); }
    void system::set_sensor_driver(drivers::sensor_driver *driver) { impl->set_sensor_driver(driver); }
    void system::set_debugger(debugger_base *dbgr) { impl->set_debugger(dbgr); }
    bool system::set_device(const std::uint8_t idx) { return impl->set_device(idx); }
    void system::set_symbian_version_use(const epocver ever) { impl->set_symbian_version_use(ever); }
    void system::set_cpu_executor_type(const arm_emulator_type type) { impl->set_cpu_executor_type(type); }
    const arm_emulator_type system::get_cpu_executor_type() const { return impl->get_cpu_executor_type(); }
    loader::rom *system::get_rom_info() { return impl->get_rom_info(); }
    epocver system::get_symbian_version_use() const { return impl->get_symbian_version_use(); }
    void system::prepare_reschedule() { impl->prepare_reschedule(); }
    void system::startup() { impl->startup(); }
    bool system::load(const std::u16string &path, const std::u16string &cmd_arg) { return impl->load(path, cmd_arg); }
    int system::loop() { return impl->loop(); }
    bool system::pause() { return impl->pause(); }
    bool system::unpause() { return impl->unpause(); }
    memory_system *system::get_memory_system() { return impl->get_memory_system(); }
    kernel_system *system::get_kernel_system() { return impl->get_kernel_system(); }
    hle::lib_manager *system::get_lib_manager() { return impl->get_lib_manager(); }
    io_system *system::get_io_system() { return impl->get_io_system(); }
    ntimer *system::get_ntimer() { return impl->get_ntimer(); }
    disasm *system::get_disasm() { return impl->get_disasm(); }
    gdbstub *system::get_gdb_stub() { return impl->get_gdb_stub(); }
    drivers::graphics_driver *system::get_graphics_driver() { return impl->get_graphic_driver(); }
    drivers::audio_driver *system::get_audio_driver() { return impl->get_audio_driver(); }
    drivers::sensor_driver *system::get_sensor_driver() { return impl->get_sensor_driver(); }
    arm::core *system::get_cpu() { return impl->get_cpu(); }
    dispatch::dispatcher *system::get_dispatcher() { return impl->get_dispatcher(); }
    void system::mount(drive_number drv, const drive_media media, std::string path, const std::uint32_t attrib) { impl->mount(drv, media, path, attrib); }
    bool system::reset() { return impl->reset(true); }
    void system::request_exit() { impl->request_exit(); }
    bool system::should_exit() const { return impl->should_exit(); }
    void system::add_new_hal(uint32_t hal_category, hal_instance &hal_com) { impl->add_new_hal(hal_category, hal_com); }
    epoc::hal *system::get_hal(uint32_t category) { return impl->get_hal(category); }
    void system::do_state(common::chunkyseri &seri) { impl->do_state(seri); }
    const language system::get_system_language() const { return impl->get_system_language(); }
    void system::set_system_language(const language new_lang) { impl->set_system_language(new_lang); }
    void system::validate_current_device() { impl->validate_current_device(); }
    std::size_t system::add_system_reset_callback(system_reset_callback_type cb) { return impl->add_system_reset_callback(cb); }
    bool system::remove_system_reset_callback(const std::size_t h) { return impl->remove_system_reset_callback(h); }
    bool system::rescan_devices(const drive_number romdrv) { return impl->rescan_devices(romdrv); }
    void system::initialize_user_parties() { impl->initialize_user_parties(); }
    bool system::is_s80_device_active() { return impl->is_s80_device_active(); }
}