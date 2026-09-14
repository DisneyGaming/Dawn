// The existing authenticated player-physics callback supplies all movement.
// Sample only the traversal phase, at most 20 times per second. The controller
// checks the authored hoop and arrival trigger; this is not floor detection.
namespace eater_arrival_motion {
SRWLOCK lock=SRWLOCK_INIT;
std::uint64_t run{},next{};
std::uint32_t attempt{},identity{UINT32_MAX};
void reset() noexcept {
    AcquireSRWLockExclusive(&lock);run=next=0;attempt=0;identity=UINT32_MAX;ReleaseSRWLockExclusive(&lock);
}
}
void observe_eater_arrival_motion(void* physics,std::uint32_t player,std::uint64_t now) noexcept {
    namespace eater=state::activity::eater_of_worlds;
    namespace gn=gateway_native;
    namespace contact=eater_platform_contact_native;
    const auto request=eater::arrival_request();
    if(!request.enabled || request.player!=player || player==UINT32_MAX || !now) return;
    bool claimed{};
    AcquireSRWLockExclusive(&eater_arrival_motion::lock);
    if(eater_arrival_motion::run!=request.owner.run || eater_arrival_motion::attempt!=request.attempt
        || eater_arrival_motion::identity!=player) {
        eater_arrival_motion::run=request.owner.run;eater_arrival_motion::attempt=request.attempt;
        eater_arrival_motion::identity=player;eater_arrival_motion::next=0;
    }
    if(now>=eater_arrival_motion::next) {eater_arrival_motion::next=now+50;claimed=true;}
    ReleaseSRWLockExclusive(&eater_arrival_motion::lock);
    if(!claimed) return;
    const auto address=reinterpret_cast<std::uintptr_t>(physics);
    if(!contact::can_add(address,0x208)) return;
    gn::Read read{g_image};std::array<std::byte,0x30> header{};
    std::uint32_t self{},controlled{};std::uintptr_t resolved{},rows{},body{};
    std::int32_t count{},index{};
    if(!read.copy(address,header)
        || !prefix(header.data(),contact::kPlayerPhysicsConfig,contact::kPhysicsKind,contact::kPlayerPhysicsOffset)
        || (self=at<std::uint32_t>(header.data()+0x24))==UINT32_MAX
        || at<std::uint32_t>(header.data()+0x2C)!=player
        || !read.resolve(self,resolved) || resolved!=address
        || !read.value(address+0x190,rows) || !read.value(address+0x198,count)
        || !read.value(address+0x204,index) || count<=0 || count>64 || index<0 || index>=count
        || !contact::can_add(rows,static_cast<std::size_t>(count)*0x50)
        || !read.value(rows+static_cast<std::uintptr_t>(index)*0x50+0x20,body)) return;
    std::array<float,4> position{};std::uintptr_t bodyVtable{};
    if(!contact::can_add(body,0x1D0) || !read.value(body,bodyVtable)
        || bodyVtable!=g_image+contact::kRigidBodyVtableRva || !read.value(body+0x1C0,position)) return;
    gn::Read terminal{g_image};std::array<std::byte,0x30> finalHeader{};
    std::uintptr_t finalRows{},finalBody{},finalResolved{};std::int32_t finalCount{},finalIndex{};
    std::array<float,4> finalPosition{};
    if(!terminal.copy(address,finalHeader) || finalHeader!=header
        || !terminal.resolve(self,finalResolved) || finalResolved!=address
        || !terminal.value(address+0x190,finalRows) || finalRows!=rows
        || !terminal.value(address+0x198,finalCount) || finalCount!=count
        || !terminal.value(address+0x204,finalIndex) || finalIndex!=index
        || !terminal.value(rows+static_cast<std::uintptr_t>(index)*0x50+0x20,finalBody) || finalBody!=body
        || !terminal.value(body+0x1C0,finalPosition) || finalPosition!=position
        || !hooks::teleport::read_local_player_entity(physics,controlled) || controlled!=player
        || eater::arrival_request()!=request) return;
    if(eater::arrival_request()!=request) return;
    static_cast<void>(eater::observe_arrival_motion({request.owner.run,now,player,request.attempt,
        {position[0],position[1],position[2]}}));
}
