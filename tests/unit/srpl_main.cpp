#include <dso/dso_transport.hpp>

#include <stdlib.h>

int main() {

    otbr::Ncp::ControllerOpenThread *ncp = nullptr;
    otbr::Dso::DsoAgent dsoAgent(*ncp);

    return 0;
}
