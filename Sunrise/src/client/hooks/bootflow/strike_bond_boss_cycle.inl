// The existing admitted-character update owns all calls. No new detours or
// writes to animation arenas, transforms, death bits, or actor AI flags.
namespace garden_cycle {
namespace policy=strike_bond_boss_cycle;
namespace gn=gateway_native;
using Animation=void(__fastcall*)(void*,const void*,void*) noexcept;
using Position=std::uint64_t(__fastcall*)(void*,float,std::uint8_t) noexcept;
using Fraction=float(__fastcall*)(void*,int) noexcept;
SRWLOCK lock=SRWLOCK_INIT;
struct Ledger {
    garden::EnemyReceipt enemy{};std::array<bool,2> sleep{},wake{},movement{},returned{};
    bool openingStarted{},openingSignaled{},openingReleased{},firstTravel{},death{},busy{};
    std::uint64_t next{},openingSignalTime{};unsigned reports{};
};
Ledger ledger{};
void reset() noexcept {ledger={};}
void report(const garden::BossRequest& r,const char* event,float position=0.F) noexcept {
    std::array<char,384> line{};
    std::snprintf(line.data(),line.size(),"ev=garden_cycle event=%s run=%llu actor=%08X cycle=%u stage=%u position=%.6f",
        event,r.owner.run,r.enemy.actor,r.frame.bossCycle.cycle,r.frame.bossStage,position);
    core::log::write(core::log::Channel::client,core::log::Level::info,line.data());
}
bool boundaries() noexcept {
    constexpr std::array<std::uint8_t,16> animation{0x48,0x89,0x5C,0x24,0x08,0x48,0x89,0x7C,0x24,0x10,0x55,0x48,0x8B,0xEC,0x48,0x83};
    gn::Read read{g_image};std::array<std::uint8_t,16> bytes{};
    for(const auto rva:{0xC620F0U,0xC693F0U}) if(!read.value(g_image+rva,bytes) || bytes!=animation) return false;
    constexpr std::array<std::uint8_t,16> getter{0x48,0x83,0xEC,0x68,0x44,0x8B,0x09,0x4C,0x8B,0xD1,0x48,0x89,0x4C,0x24,0x28,0x41};
    namespace lattice=omega_vex_lattice_probe::detail;
    const auto original=lattice::positionOriginal.load(std::memory_order_acquire);
    return policy::position_boundary(read,g_image+0xDF6C70,reinterpret_cast<std::uintptr_t>(original),
        reinterpret_cast<std::uintptr_t>(&lattice::position),lattice::handles[0].attached && lattice::callGate.accepting())
        && read.value(g_image+0xCD6C20,bytes) && bytes==getter;
}
void dispatch(const policy::Binding& b,std::uint32_t sequence,std::uint32_t gate=0,bool remove=false) noexcept {
    const auto action=policy::action(sequence,gate);
    reinterpret_cast<Animation>(g_image+(remove?0xC693F0:0xC620F0))(reinterpret_cast<void*>(b.controller),action.data(),nullptr);
}
bool burst_ready() noexcept {
    gn::Read read{g_image};std::uintptr_t entity{},graph{};std::uint64_t size{};
    return read.resolve(policy::kBurst,entity) && read.value(entity,size) && size==2244
        && read.resolve(0x80F45BA5U,graph) && read.value(graph,size) && size==6560;
}
void burst(const policy::Binding& b) noexcept {
    // Exact native event shape verified in the live second-Minotaur test.
    alignas(16) std::array<std::byte,32> event{},context{};
    const std::uint32_t marker=0x811C9DC5U,absent=UINT32_MAX;
    event[2]=std::byte{3};std::memcpy(event.data()+8,&marker,4);std::memcpy(event.data()+24,&policy::kBurst,4);
    std::memcpy(context.data(),&b.animatorSelf,4);std::memcpy(context.data()+8,&absent,4);
    using Effect=void(__fastcall*)(const void*,const void*,void*,std::uint8_t) noexcept;
    reinterpret_cast<Effect>(g_image+0x104DA00)(event.data(),context.data(),reinterpret_cast<void*>(b.animator),0);
}
void after_update(void* character) noexcept {
    const auto r=garden::boss_request();if(!policy::wanted(r)) return;
    AcquireSRWLockExclusive(&lock);
    if(ledger.busy) {ReleaseSRWLockExclusive(&lock);return;}
    if(ledger.enemy!=r.enemy) {reset();ledger.enemy=r.enemy;}
    const auto now=GetTickCount64();const bool claim=!ledger.busy && now>=ledger.next;
    if(claim) {ledger.busy=true;ledger.next=now+50;}
    ReleaseSRWLockExclusive(&lock);if(!claim)return;
    struct Release {~Release(){AcquireSRWLockExclusive(&lock);ledger.busy=false;ReleaseSRWLockExclusive(&lock);}} release;
    policy::Binding b{};gn::Read read{g_image};const auto address=reinterpret_cast<std::uintptr_t>(character);
    if(!policy::sample(read,g_image,address,r,b)) {
        if(ledger.reports++<4) report(r,"identity_guard");return;
    }
    if(!boundaries()) {
        if(ledger.reports++<4) report(r,"native_boundary_guard");return;
    }
    float position{},velocity{};std::uint8_t healthFlags{};std::array<std::uint32_t,3> flags{};
    if(!read.value(b.device+0x370,position) || !garden::boss_position(position)
        || !read.value(b.device+0x374,velocity) || !std::isfinite(velocity)
        || !read.value(b.health+0x338,healthFlags)) return;
    for(unsigned i=0;i<3;++i) if(!read.value(b.selector+0xB4+64*i,flags[i])) return;
    if(!garden::observe_boss_motion(r.enemy,r.platform,position)) return;
    // Revalidate all leased components before invoking native code.
    const auto current=garden::boss_request();policy::Binding again{};gn::Read checked{g_image};
    if(current.owner!=r.owner || current.enemy!=r.enemy || current.platform!=r.platform
        || current.frame.bossCycle.mode!=r.frame.bossCycle.mode || current.frame.bossCycle.cycle!=r.frame.bossCycle.cycle
        || !policy::sample(checked,g_image,address,current,again) || again!=b) return;
    const auto& cycle=current.frame.bossCycle;const auto number=cycle.cycle;
    if(cycle.mode==garden::BossMode::opening) {
        if(flags[0] || flags[2]) return;
        if(!ledger.openingStarted) {
            // garden_intro releases the stale pre-cube hold first. Replay the
            // authored load/unfold sequence with clean start and exit inputs.
            if(flags[1]) return;
            dispatch(b,policy::kIntro,policy::kIntroStart,true);
            dispatch(b,policy::kIntro,policy::kIntroExit,true);
            dispatch(b,policy::kIntro);
            ledger.openingStarted=true;
            garden::observe_boss_animation(r.enemy,0,garden::BossAnimation::openingStarted);
            report(current,"opening_started",position);return;
        }
        if(!ledger.openingSignaled) {
            if(!flags[1] || !burst_ready()) return;
            dispatch(b,policy::kIntro,policy::kIntroStart);
            dispatch(b,policy::kIntro,policy::kIntroExit);burst(b);
            ledger.openingSignaled=true;ledger.openingSignalTime=now;
            report(current,"opening_start_and_exit_armed",position);return;
        }
        // This selector can retain its intro hold after presentation. Use the
        // named stop verified live, once, after an eight-second presentation
        // window. Time never fabricates completion: the following update must
        // observe the native selector inactive before combat can start.
        if(policy::release_opening_hold(ledger.openingStarted,ledger.openingSignaled,ledger.openingReleased,
            flags[1]!=0,ledger.openingSignalTime,now)) {
            ledger.openingReleased=true;dispatch(b,policy::kIntro,0,true);
            report(current,"opening_hold_released",position);return;
        }
        if(!flags[1] && garden::observe_boss_animation(r.enemy,0,garden::BossAnimation::openingAwake))
            report(current,"opening_complete",position);
    } else if(cycle.mode==garden::BossMode::damage && number==0 && cycle.hasFirstTarget) {
        if(!ledger.firstTravel) {
            const auto origin=garden::boss_parking_origin(position,cycle.firstTarget);
            const auto move=reinterpret_cast<Position>(g_image+0xDF6C70);
            if(origin!=position) move(reinterpret_cast<void*>(b.device),origin,1);
            move(reinterpret_cast<void*>(b.device),cycle.firstTarget,0);
            ledger.firstTravel=true;report(current,"first_damage_turn_started",cycle.firstTarget);
        }
    } else if(cycle.mode==garden::BossMode::damage && number>=1 && number<=2 && cycle.hasReturnTarget) {
        const auto i=number-1;
        if(!ledger.returned[i]) {
            const auto origin=garden::boss_parking_origin(position,cycle.returnTarget);
            const auto move=reinterpret_cast<Position>(g_image+0xDF6C70);
            if(origin!=position) move(reinterpret_cast<void*>(b.device),origin,1);
            move(reinterpret_cast<void*>(b.device),cycle.returnTarget,0);
            ledger.returned[i]=true;report(current,"return_home_started",cycle.returnTarget);
        }
    } else if(cycle.mode==garden::BossMode::parking && number>=1 && number<=2) {
        const auto i=number-1;
        if(!ledger.movement[i]) {
            if(!cycle.hasParkTarget) return;
            const auto origin=garden::boss_parking_origin(position,cycle.parkTarget);
            const auto move=reinterpret_cast<Position>(g_image+0xDF6C70);
            // Rebase only an equivalent native phase. The visible turn begins
            // immediately at normal speed and never snaps to its destination.
            if(origin!=position) move(reinterpret_cast<void*>(b.device),origin,1);
            move(reinterpret_cast<void*>(b.device),cycle.parkTarget,0);
            ledger.movement[i]=true;report(current,"parking_turn_started",cycle.parkTarget);
        }
        if(!ledger.sleep[i]) {
            if(flags[1] || flags[2]) return;
            // Native gate references survive sequence completion. Remove our
            // previous pair before starting the next hold so it cannot wake early.
            dispatch(b,policy::kSleep,policy::kGate1,true);dispatch(b,policy::kSleep,policy::kGate2,true);
            if(!flags[0]) dispatch(b,policy::kSleep);
            ledger.sleep[i]=true;report(current,"sleep_requested",position);return;
        }
        if(flags[0] && !cycle.asleep) garden::observe_boss_animation(r.enemy,number,garden::BossAnimation::asleep);
        if(flags[0] && cycle.hasParkTarget && garden::boss_at(position,cycle.parkTarget) && std::abs(velocity)<.0001F
            && garden::observe_boss_animation(r.enemy,number,garden::BossAnimation::parked)) report(current,"parked",position);
    } else if(cycle.mode==garden::BossMode::waking && number>=1 && number<=2) {
        const auto i=number-1;
        if(!ledger.wake[i]) {
            if(!flags[0] || flags[1] || flags[2] || !burst_ready()) return;
            dispatch(b,policy::kSleep,policy::kGate1);dispatch(b,policy::kSleep,policy::kGate2);burst(b);
            ledger.wake[i]=true;
            garden::observe_boss_animation(r.enemy,number,garden::BossAnimation::wakeStarted);
            report(current,"wake_and_burst",position);
        } else if(!flags[0] && garden::observe_boss_animation(r.enemy,number,garden::BossAnimation::awake)) {
            report(current,"awake_resume",position);
        }
    } else if(cycle.mode==garden::BossMode::dying && !ledger.death) {
        if((healthFlags&1U) || flags[0] || flags[1] || flags[2] || current.frame.bossStage!=2) return;
        const float fraction=reinterpret_cast<Fraction>(g_image+0xCD6C20)(reinterpret_cast<void*>(b.health),0);
        if(fraction!=0.F)return;
        // Freeze the live channel at its present position before starting the
        // death clip. The platform object and the native parent remain intact.
        reinterpret_cast<Position>(g_image+0xDF6C70)(reinterpret_cast<void*>(b.device),position,1);
        if(!garden::observe_boss_animation(r.enemy,number,garden::BossAnimation::deathStarted)) return;
        ledger.death=true;dispatch(b,policy::kDeath);report(current,"death_platform_stopped",position);
        // Only the authentic native death callback can finish the mission.
    }
}
}
