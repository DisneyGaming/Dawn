#include "ambient_population_named_points.h"
#include <mutex>
namespace sunrise::server::runtime::activity::ambient_population::named_points {
namespace {std::mutex mutex;Mailbox mailbox;}
bool bind(const Ticket& value) noexcept {std::lock_guard lock(mutex);return mailbox.bind(value);}
Snapshot lookup(std::uint32_t list) noexcept {std::lock_guard lock(mutex);return mailbox.lookup(list);}
Snapshot retirement(std::uint32_t list) noexcept {std::lock_guard lock(mutex);return mailbox.retirement(list);}
Snapshot retirement(std::uint32_t list,std::uint32_t handle) noexcept {
    std::lock_guard lock(mutex);return mailbox.retirement(list,handle);
}
bool ready(const Ticket& value) noexcept {
    std::lock_guard lock(mutex);const auto state=mailbox.lookup(value.list);
    return state.binding.ticket==value && state.ready();
}
bool constructed(const Binding& binding,const Point& point,std::uint32_t handle) noexcept {
    std::lock_guard lock(mutex);return mailbox.constructed(binding,point,handle);
}
bool interface_resolved(const Binding& binding,std::uint64_t guid) noexcept {
    std::lock_guard lock(mutex);return mailbox.interface_resolved(binding,guid);
}
bool removing(const Binding& binding) noexcept {std::lock_guard lock(mutex);return mailbox.removing(binding);}
bool actor_retired(const Binding& binding,const Point& point,std::uint32_t handle) noexcept {
    std::lock_guard lock(mutex);return mailbox.actor_retired(binding,point,handle);
}
void release(Owner owner) noexcept {std::lock_guard lock(mutex);mailbox.release(owner);}
}
