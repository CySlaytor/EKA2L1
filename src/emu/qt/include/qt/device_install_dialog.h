#ifndef DEVICE_INSTALL_DIALOG_H
#define DEVICE_INSTALL_DIALOG_H

#include <QDialog>
#include <atomic>
#include <string>
#include <vector>

namespace Ui {
    class device_install_dialog;
}

namespace eka2l1 {
    namespace config {
        struct state;
    }

    class device_manager;
}

class device_install_dialog : public QDialog {
    Q_OBJECT

private:
    eka2l1::config::state &conf_;
    eka2l1::device_manager *device_mngr_;

    std::atomic<bool> canceled_;

private slots:
    void on_current_index_changed(int new_index);
    void on_rom_browse_triggered();
    void on_install_triggered();
    void on_cancel_triggered();
    void on_progress_bar_update(const std::size_t so_far, const std::size_t total);

signals:
    void progress_bar_update(const std::size_t so_far, const std::size_t total);
    void new_device_added();

public:
    explicit device_install_dialog(QWidget *parent, eka2l1::device_manager *dvcmngr, eka2l1::config::state &conf);
    ~device_install_dialog();

private:
    Ui::device_install_dialog *ui;
};

#endif // DEVICE_INSTALL_DIALOG_H