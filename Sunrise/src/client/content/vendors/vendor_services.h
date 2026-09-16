#pragma once
#include "../../../middleware/content/packages/reader/reader.h"
namespace sunrise::client::content::vendors {
bool build_services(const middleware::content::packages::reader::Source&,
    middleware::content::packages::reader::Scratch&) noexcept;
}
