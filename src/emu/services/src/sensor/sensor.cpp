#include <services/sensor/sensor.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1 {
    sensor_server::sensor_server(eka2l1::system *sys)
        : service::typical_server(sys, "!SensorServer") {
    }
}