#pragma once
namespace dawn::client::hooks::bootflow::gateway_patrol {
bool install() noexcept;
void quiesce() noexcept;
bool uninstall() noexcept;
}
