#pragma once

#include "../../client/network/consumer.h"

namespace dawn::server::http {

/** Routes an HTTP request to an in-process Server handler. */
[[nodiscard]] bool consume(const client::network::HttpRequest& request,
                           client::network::HttpResponse& response) noexcept;

} // namespace dawn::server::http
