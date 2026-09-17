namespace lost_sector_native_object {
namespace moon=server::runtime::activity::moon_lost_sector;
namespace nativeActivity=server::runtime::activity::native_activity;
namespace destructible=server::runtime::activity::lost_sector_destructible;
inline constexpr auto& kDefinitions=destructible::kBindings;

void observe_source(void* raw) noexcept {
    gateway_native::Read read{g_image};const auto source=reinterpret_cast<std::uintptr_t>(raw);
    std::array<std::byte,16> header{};if(!read.copy(source,header))return;
    const auto definition=at<std::uint32_t>(header.data());
    const auto* binding=destructible::binding(definition);
    if(!binding
        || !prefix(header.data(),definition,0x80809928U,0x4C8U))return;
    const auto activity=state::activity::newest_joined_activity();
    const auto request=nativeActivity::lost_sector_object_request(activity,definition);if(!request.valid())return;
    std::uint32_t generation{},committed{},bundle{};std::uint8_t active{};gateway_native::Weak entity{},after{};
    if(!read.value(source+0x180,generation) || generation!=request.owner.value
        || !read.value(source+0x2F0,committed) || committed!=generation
        || !read.value(source+0x188,active) || active!=1 || !read.value(source+0x440,entity))return;
    std::uintptr_t row{};if(!read.entity_row(entity,row) || !read.value(row+0x4C,bundle))return;
    gateway_module_native_path::Probe probe{};
    if(!gateway_module_native_path::find(read,bundle,entity.handle,probe,
        {binding->healthDefinition,0x80804B8AU,binding->healthOffset}))return;
    std::uint32_t afterGeneration{};
    if(!read.value(source+0x440,after) || after!=entity || !read.weak(after)
        || !read.value(source+0x180,afterGeneration) || afterGeneration!=generation
        || !read.copy(source,header) || !prefix(header.data(),definition,0x80809928U,0x4C8U))return;
    const moon::ObjectReceipt receipt{request.activity,request.boot,request.owner,request.source,
        source,entity.handle,entity.serial,probe.health};
    const bool replaced=request.binding.valid() && request.binding!=receipt
        && !read.weak({request.binding.serial,request.binding.entity});
    static_cast<void>(nativeActivity::lost_sector_object_observed(receipt,probe.dead,replaced));
}
[[nodiscard]] bool current(gateway_native::Read& read,const moon::DestructibleRequest& request,
    const native_box_identity::Sample& sample) noexcept {
    if(!request.valid() || !request.binding.valid() || request.binding.activity!=request.activity
        || request.binding.boot!=request.boot || request.binding.owner!=request.owner
        || request.binding.source!=request.source)return false;
    return native_box_identity::current(read,{request.binding.sourceAddress,request.owner.value,
        request.binding.serial,request.binding.entity,request.binding.health},sample,request.source.definition);
}
[[nodiscard]] bool blocked(const void* context) noexcept {
    gateway_native::Read read{g_image};native_box_identity::Sample sample{};
    for(const auto& definition:kDefinitions) {
        if(!native_box_identity::sample(read,reinterpret_cast<std::uintptr_t>(context),sample,
            {definition.healthDefinition,0x80804B8AU,definition.healthOffset}))continue;
        const auto request=nativeActivity::lost_sector_object_request(
            state::activity::newest_joined_activity(),definition.source);
        if(current(read,request,sample))return !request.vulnerable && !request.destroyed;
    }
    return false;
}
[[nodiscard]] bool allowed(const void* context,bool nativeResult) noexcept {
    gateway_native::Read read{g_image};native_box_identity::Sample sample{};
    const auto activity=state::activity::newest_joined_activity();
    for(const auto& definition:kDefinitions) {
        if(!native_box_identity::sample(read,reinterpret_cast<std::uintptr_t>(context),sample,
            {definition.healthDefinition,0x80804B8AU,definition.healthOffset}))continue;
        const auto request=nativeActivity::lost_sector_object_request(activity,definition.source);
        if(current(read,request,sample))return request.vulnerable && !request.destroyed;
    }
    return nativeResult;
}
void receipt(const void* context) noexcept {
    gateway_native::Read read{g_image};native_box_identity::Sample sample{};
    for(const auto& definition:kDefinitions) {
        if(!native_box_identity::sample(read,reinterpret_cast<std::uintptr_t>(context),sample,
            {definition.healthDefinition,0x80804B8AU,definition.healthOffset}) || !sample.dead)continue;
        const auto request=nativeActivity::lost_sector_object_request(
            state::activity::newest_joined_activity(),definition.source);
        if(request.vulnerable && !request.destroyed && current(read,request,sample)) {
            static_cast<void>(nativeActivity::lost_sector_object_observed(request.binding,true));return;
        }
    }
}
}
