#include "replication_roles.h"
#include <mutex>
namespace dawn::state::gameplay::replication {
namespace {std::mutex g_lock;Roles g_roles;}
void begin_control_host_epoch(std::uint64_t epoch) noexcept {std::lock_guard lock(g_lock);g_roles.begin(epoch);}
bool publish_control_host(std::uint64_t machine,const Address& address) noexcept {
    std::lock_guard lock(g_lock);return g_roles.publish(machine,address);
}
std::uint64_t control_host_epoch(std::uint64_t machine,const Address& address) noexcept {
    std::lock_guard lock(g_lock);return g_roles.lookup(machine,address);
}
}
