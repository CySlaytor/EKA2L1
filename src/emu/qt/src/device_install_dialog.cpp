#include "ui_device_install_dialog.h"
#include <qt/device_install_dialog.h>

#include <common/fileutils.h>
#include <common/log.h>
#include <common/path.h>

#include <config/config.h>
#include <loader/rom.h>
#include <system/devices.h>

#include <functional>
#include <thread>

#include <QFileDialog>
#include <QFuture>
#include <QtConcurrent/QtConcurrent>

#include <QInputDialog>
#include <QMessageBox>

namespace eka2l1 {
    using progress_changed_callback = std::function<void(std::size_t, std::size_t)>;
    using cancel_requested_callback = std::function<bool()>;

    enum device_installation_error {
        device_installation_none = 0,
        device_installation_already_exist,
        device_installation_determine_product_failure,
        device_installation_fpsx_corrupt,
        device_installation_general_failure,
        device_installation_insufficent,
        device_installation_not_exist,
        device_installation_rofs_corrupt,
        device_installation_rom_fail_to_copy,
        device_installation_rom_file_corrupt,
        device_installation_rpkg_corrupt,
        device_installation_vpl_file_invalid
    };

    namespace loader {
        device_installation_error install_rom(
            device_manager *dvcmngr,
            const std::string &rom_path,
            const std::string &rom_resident_path,
            const std::string &root_z_path,
            progress_changed_callback progress_cb,
            cancel_requested_callback cancel_cb) {
            return device_installation_general_failure;
        }
    }
}

static constexpr std::int32_t INSTALL_METHOD_DEVICE_DUMP_INDEX = 0;

device_install_dialog::device_install_dialog(QWidget *parent, eka2l1::device_manager *dvcmngr, eka2l1::config::state &conf)
    : QDialog(parent)
    , conf_(conf)
    , device_mngr_(dvcmngr)
    , canceled_(false)
    , ui(new Ui::device_install_dialog) {
    setWindowFlags(windowFlags() & ~Qt::WindowCloseButtonHint);
    setAttribute(Qt::WA_DeleteOnClose);

    ui->setupUi(this);

    ui->confirmation_install_btn->setDisabled(true);
    ui->install_progress_bar->setVisible(false);

    on_current_index_changed(ui->installation_combo->currentIndex());

    connect(ui->installation_combo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &device_install_dialog::on_current_index_changed);
    connect(ui->rom_browse_btn, &QPushButton::clicked, this, &device_install_dialog::on_rom_browse_triggered);
    connect(ui->confirmation_install_btn, &QPushButton::clicked, this, &device_install_dialog::on_install_triggered);
    connect(ui->confirmation_cancel_btn, &QPushButton::clicked, this, &device_install_dialog::on_cancel_triggered);
    connect(this, &device_install_dialog::progress_bar_update, this, &device_install_dialog::on_progress_bar_update, Qt::QueuedConnection);
}

device_install_dialog::~device_install_dialog() {
    delete ui;
}

void device_install_dialog::on_cancel_triggered() {
    canceled_ = true;

    if (!ui->install_progress_bar->isVisible()) {
        close();
    }
}

void device_install_dialog::on_current_index_changed(int new_index) {
    ui->vpl_browse_widget->setVisible(false);
    ui->rpkg_browse_widget->setVisible(false);
    ui->rom_browse_widget->setVisible(true);
}

void device_install_dialog::on_progress_bar_update(const std::size_t so_far, const std::size_t total) {
    ui->install_progress_bar->setValue(static_cast<int>(so_far * 100 / total));
}

void device_install_dialog::on_install_triggered() {
    ui->installation_choose_widget->setVisible(false);
    ui->install_progress_bar->setVisible(true);
    ui->confirmation_install_btn->setDisabled(true);

    QFuture<eka2l1::device_installation_error> install_future = QtConcurrent::run([this]() {
        const std::string root_z_path = eka2l1::add_path(conf_.storage, "drives/z/");
        const std::string rom_resident_path = eka2l1::add_path(conf_.storage, "roms/");

        eka2l1::common::create_directories(rom_resident_path);
        eka2l1::device_installation_error error = eka2l1::device_installation_none;

        auto progress_update_cb_func = [this](const std::size_t taken, const std::size_t total) {
            emit progress_bar_update(taken, total);
        };

        auto cancel_cb_func = [this]() {
            return canceled_.load();
        };

        if (ui->rom_browse_widget->isVisible()) {
            error = eka2l1::loader::install_rom(device_mngr_, ui->rom_path_line_edit->text().toStdString(), rom_resident_path, root_z_path, progress_update_cb_func, cancel_cb_func);
        }

        if (error != eka2l1::device_installation_none) {
            return error;
        }

        device_mngr_->save_devices();
        return error;
    });

    while (!install_future.isFinished()) {
        QCoreApplication::processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (!canceled_) {
        QWidget *leecher = parentWidget();

        if (install_future.result() == eka2l1::device_installation_none) {
            eka2l1::device *lastest = device_mngr_->lastest();
            QMessageBox::information(leecher, tr("Installation completed"),
                tr("Device %1 (%2) has been successfully installed!").arg(QString::fromStdString(lastest->model), QString::fromStdString(lastest->firmware_code)));

            emit new_device_added();
        } else {
            QString error_string;

            switch (install_future.result()) {
            case eka2l1::device_installation_already_exist:
                error_string = tr("The device has already been installed!");
                break;

            case eka2l1::device_installation_determine_product_failure:
                error_string = tr("Fail to determine product information from the dump!");
                break;

            case eka2l1::device_installation_general_failure:
                error_string = tr("An unknown error has occured. Please contact the developers with your log!");
                break;

            case eka2l1::device_installation_insufficent:
                error_string = tr("The disk space is insufficient to perform the device install! Please free your disk space!");
                break;

            case eka2l1::device_installation_not_exist:
                error_string = tr("Some files provided for installation do not exist anymore. Please keep them intact until the installation is done!");
                break;

            case eka2l1::device_installation_rom_fail_to_copy:
                error_string = tr("Fail to copy ROM file!");
                break;

            case eka2l1::device_installation_rom_file_corrupt:
                error_string = tr("The provided ROM is corrupted! Please make sure your ROM is valid!");
                break;

            default:
                break;
            }

            if (!error_string.isEmpty()) {
                QMessageBox::critical(leecher, tr("Installation failed"), error_string);
            }
        }
    }

    close();
}

void device_install_dialog::on_rom_browse_triggered() {
    QString rom_file_path = QFileDialog::getOpenFileName(this, tr("Choose the ROM"),
        QString(), tr("ROM file (*.rom *.ROM);;All files (*.*)"));

    if (!rom_file_path.isEmpty()) {
        ui->rom_path_line_edit->setText(rom_file_path);
        const std::string rom_file_path_std = rom_file_path.toStdString();

        if (eka2l1::common::exists(rom_file_path_std)) {
            ui->rpkg_browse_widget->setVisible(false);
            ui->confirmation_install_btn->setDisabled(false);
        }
    }
}