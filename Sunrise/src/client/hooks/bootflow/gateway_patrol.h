#pragma once
namespace sunrise::client::hooks::bootflow::gateway_patrol {
bool install() noexcept;
void quiesce() noexcept;
bool uninstall() noexcept;
}
