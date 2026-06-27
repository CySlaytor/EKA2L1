#include <services/ui/view/view.h>
#include <system/epoc.h>
#include <utils/err.h>

namespace eka2l1 {
    std::string get_view_server_name_by_epocver(const epocver ver) {
        if (ver <= epocver::epoc80) {
            return "ViewServer";
        }
        return "!ViewServer";
    }
}