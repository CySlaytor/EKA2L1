#include <services/etel/subsess.h>

namespace eka2l1 {
    etel_subsession::etel_subsession(etel_session *session, const etel_legacy_level lvl)
        : session_(session)
        , legacy_level_(lvl) {
    }
}