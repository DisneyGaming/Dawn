// Acknowledged poses need no device poll until the controller issues another
// revision. The native DF6510 consumer remains the only pose writer.
void observe_eater_platform_pose(std::size_t assetIndex,
    const eater_platform_contacts::Snapshot& saved,
    const state::activity::eater_of_worlds::ObjectRequest& request) noexcept {
    namespace native=eater_platform_contacts;namespace eater=native::eater;namespace gn=native::gn;
    if(!request.enabled || !request.poseRevision || request.poseAcknowledged) return;
    const auto& slot=saved.slot;const auto device=slot.components.device.address;
    const auto index=eater::platform_index(slot.object.source);
    if(index>=native::slots.size()) return;
    gn::Read read{g_image};std::array<std::byte,16> pose{},again{};std::uint32_t revision{},finalRevision{};
    if(!native::contact::can_add(device,0x964)
        || !native::cache::current_component(read,slot.components.device,slot.object.entity,native::deviceDefinition)
        || !read.copy(device+0x370,pose) || !read.value(device+0x960,revision)
        || revision!=request.poseRevision) return;
    const auto actual=at<float>(pose.data()),target=at<float>(pose.data()+12);
    if(!std::isfinite(actual) || !std::isfinite(target) || actual!=target
        || target!=(request.raised?1.F:0.F)) return;
    if(!native::cache::current_component(read,slot.components.device,slot.object.entity,native::deviceDefinition)
        || !native::source_current(read,slot,request,assetIndex)
        || !read.copy(device+0x370,again) || at<float>(again.data())!=actual
        || at<float>(again.data()+12)!=target
        || !read.value(device+0x960,finalRevision) || finalRevision!=revision
        || eater::object_request(assetIndex)!=request || !native::retained(index,saved)) return;
    static_cast<void>(eater::observe_platform_pose({slot.object,revision,target}));
}
