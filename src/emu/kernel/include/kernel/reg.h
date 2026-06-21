#pragma once

#define ADD_SVC_REGISTERS(mngr, map) mngr.svc_funcs_.insert(map.begin(), map.end())

namespace eka2l1::hle {
    class lib_manager;
}

namespace eka2l1::epoc {
    // Retain only OS 9.3 SVC registration
    void register_epocv93(hle::lib_manager &mngr);
}