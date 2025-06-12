#pragma once

#ifndef HXCPP_H
#include <hxcpp.h>
#endif

#include "winhttp/WinHttp.h"

namespace linc {
    namespace winhttp {

        ::Dynamic sendHttpRequest(int requestId, ::String domain, int port, bool https, ::String path, int method, ::String body, ::String headers, ::String proxy, int timeout);

    }
}
