#pragma once
#include "source.h"
namespace dawn::client::content::activity {
[[nodiscard]] bool build_catalog(const packages::reader::Source& source,
                                 packages::reader::Scratch& scratch) noexcept;
}
