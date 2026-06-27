#include <services/audio/alf/alf.h>
#include <system/epoc.h>

namespace eka2l1 {
    alf_streamer_server::alf_streamer_server(eka2l1::system *sys)
        : service::typical_server(sys, "alfstreamerserver") {
    }
}