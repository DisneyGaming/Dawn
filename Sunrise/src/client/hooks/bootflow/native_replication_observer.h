#pragma once
namespace sunrise::client::hooks::bootflow::native_replication_observer {
// Separates server-published control-only hosts from native physics peers.
// Unmatched peers retain normal native registration and acknowledgements.
[[nodiscard]] bool install() noexcept;
void quiesce() noexcept;
[[nodiscard]] bool uninstall() noexcept;
}
