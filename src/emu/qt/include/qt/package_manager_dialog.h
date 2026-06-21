#ifndef PACKAGE_MANAGER_DIALOG_H
#define PACKAGE_MANAGER_DIALOG_H

#include <QDialog>

namespace Ui {
    class package_manager_dialog;
}

class package_manager_dialog : public QDialog {
    Q_OBJECT

public:
    explicit package_manager_dialog(QWidget *parent = nullptr);
    ~package_manager_dialog();

signals:
    void package_uninstalled();

private:
    Ui::package_manager_dialog *ui_;
};

#endif // PACKAGE_MANAGER_DIALOG_H