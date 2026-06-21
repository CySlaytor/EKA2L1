#ifndef APPLISTWIDGET_H
#define APPLISTWIDGET_H

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

#include <functional>
#include <mutex>

#include <common/sync.h>

namespace eka2l1 {
    class applist_server;
    class fbs_server;
    class io_system;

    struct apa_app_registry;
    class device_manager;

    namespace kernel {
        class process;
    }

    namespace config {
        struct state;
    }
}

class applist_widget_item : public QListWidgetItem {
public:
    int registry_index_;

    applist_widget_item(const QIcon &icon, const QString &name, int registry_index, QListWidget *listview);
};

class applist_search_bar : public QWidget {
    Q_OBJECT;

private:
    QLabel *search_label_;
    QLineEdit *search_line_edit_;
    QHBoxLayout *search_layout_;

private slots:
    void on_search_bar_content_changed(QString content);

signals:
    void new_search(QString content);

public:
    applist_search_bar(QWidget *parent = nullptr);
    ~applist_search_bar();

    const QString value() const;
};

class applist_device_combo : public QWidget {
    Q_OBJECT;

private:
    QLabel *device_label_;
    QComboBox *device_combo_;
    QHBoxLayout *device_layout_;
    int current_index_;

private slots:
    void on_device_combo_changed(int index);

signals:
    void device_combo_changed(int index);

public:
    applist_device_combo(QWidget *parent = nullptr);
    ~applist_device_combo();

    void update_devices(const QStringList &devices, const int index);
};

class applist_widget : public QWidget {
    Q_OBJECT;

public:
    applist_search_bar *search_bar_;
    applist_device_combo *device_combo_bar_;
    QListWidget *list_widget_;
    QVBoxLayout *layout_;
    QWidget *bar_widget_;
    QWidget *loading_widget_;
    QMovie *loading_gif_;
    QLabel *loading_label_;

    QLabel *no_app_visible_normal_label_;
    QLabel *no_app_visible_hide_sysapp_label_;

    eka2l1::applist_server *lister_;
    eka2l1::fbs_server *fbss_;
    eka2l1::io_system *io_;

    bool hide_system_apps_;
    bool scanning_;
    bool loaded_;

    bool should_dead_;
    std::mutex exit_mutex_;
    eka2l1::common::event scanning_done_evt_;

    eka2l1::config::state &conf_;

    void add_registeration_item_native(eka2l1::apa_app_registry &reg, const int index);

    eka2l1::apa_app_registry *get_registry_from_widget_item(applist_widget_item *item);

    void hide_all();
    void show_all();
    void on_search_content_changed(QString content);
    void update_devices(const QStringList &devices, const int index);
    void reload_whole_list();
    void show_no_apps_avail();

private slots:
    void on_list_widget_item_clicked(QListWidgetItem *item);
    void on_device_change_request(int index);
    void on_new_registeration_item_come(QListWidgetItem *item);

signals:
    void app_launch(applist_widget_item *item);
    void device_change_request(int index);
    void new_registeration_item_come(QListWidgetItem *item);

public:
    explicit applist_widget(QWidget *parent, eka2l1::applist_server *lister, eka2l1::fbs_server *fbss, eka2l1::io_system *io,
        eka2l1::config::state &conf, const bool hide_system_apps = true, const bool ngage_mode = false);
    ~applist_widget();

    bool launch_from_widget_item(applist_widget_item *item, std::function<void(eka2l1::kernel::process *)> done_cb);
    std::string get_app_name_from_widget_item(applist_widget_item *item);
    void set_hide_system_apps(const bool should_hide);
    void update_devices(eka2l1::device_manager *mngr);
    void request_reload();
};

#endif // APPLISTWIDGET_H