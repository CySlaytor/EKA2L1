#include "ui_package_manager_dialog.h"
#include <qt/package_manager_dialog.h>

package_manager_dialog::package_manager_dialog(QWidget *parent)
    : QDialog(parent)
    , ui_(new Ui::package_manager_dialog) {
    ui_->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
}

package_manager_dialog::~package_manager_dialog() {
    delete ui_;
}