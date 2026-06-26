#include <QApplication>
#include <common/platform.h>
#include <qt/state.h>
#include <qt/thread.h>

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    QCoreApplication::setOrganizationName("EKA2L1");
    QCoreApplication::setApplicationName("EKA2L1");

    eka2l1::desktop::emulator emulator_state;
    return eka2l1::desktop::emulator_entry(a, emulator_state, argc, const_cast<const char **>(argv));
}