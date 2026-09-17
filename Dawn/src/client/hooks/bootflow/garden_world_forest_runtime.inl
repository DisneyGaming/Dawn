// Included in the existing Forest hook translation unit, alongside the Omega and
// Beyond runtimes. Every pointer is re-resolved on this native tick.
//
// The Garden owner and exact Forest C configuration gate this adapter. The
// supplied archive records configuration 80F4E01E on the live worker; 80F4E01F
// names the extracted worker definition. Class 80804FEC is required as well.
// Omega and Beyond retain sole ownership of their own workers.
//
// Diagnostic rationale comes from the archive's earlier experiment, not a
// live test of this revision: its worker ticked while the destination override
// was inactive. Log every predicate on the first 24 ticks so a declined
// worker remains distinguishable from a callback that never ran.
namespace garden_forest_runtime {
namespace garden=state::activity::strike_bond;
inline constexpr std::uint32_t kWorkerDefinitionClass=0x80804FECU;
inline constexpr std::size_t kWorkerBytes=0x9BDU;
// Archive-observed Forest C configuration. Exact matching prevents this
// adapter from repairing another worker in a shared Forest destination.
inline constexpr std::uint32_t kForestC=0x80F4E01EU;

/** Every input the gate consults, so a refusal names its own reason. */
struct Gate {
    bool overrideActive{},packageMatch{},seedArmed{},worldLive{},ownerValid{},frameEnabled{},notFinished{};
    bool workerReadable{},classMatch{},notBeyond{},notOmega{};
    std::uint32_t configuration{},owner{UINT32_MAX};
    std::array<char,32> package{};std::size_t packageLength{};
    // The forced-destination override is NOT active while the forest worker ticks: the archive reports a prior run
    // with override=0 with an empty package name on all 24 ticks, while every mission-scoped
    // predicate was already true. So the override cannot gate this, and it does not need to -
    // strike_bond::request() having a valid owner on an enabled, unfinished frame is a strictly
    // stronger statement than "the forced destination says strike_bond". Both are still recorded
    // in the log line so a change of behaviour stays visible.
    bool selected() const noexcept {
        return seedArmed && worldLive && ownerValid && frameEnabled && notFinished;
    }
    bool worker() const noexcept { return workerReadable && classMatch && configuration==kForestC && notBeyond && notOmega; }
};

inline Gate evaluate(void* instance) noexcept {
    Gate g{};
    state::activity::forced::ForcedDestination destination{};
    state::activity::forced::snapshot(destination);
    g.overrideActive=state::activity::forced::override_active();
    if(destination.packageNameLength<=destination.packageName.size()) {
        g.packageLength=destination.packageNameLength<g.package.size()-1?destination.packageNameLength:g.package.size()-1;
        for(std::size_t i=0;i<g.packageLength;++i) { g.package[i]=destination.packageName[i]; }
        g.packageMatch=std::string_view(destination.packageName.data(),destination.packageNameLength)==garden::kPackage;
    }
    g.seedArmed=state::activity::mission_seed_armed();
    g.worldLive=state::activity::world_phase()!=state::activity::WorldPhase::idle;
    const auto request=garden::request();
    g.ownerValid=request.owner.valid();
    g.frameEnabled=request.frame.enabled;
    g.notFinished=!request.frame.finished;

    auto* bytes=static_cast<const std::byte*>(instance);
    g.workerReadable=bytes && readable(bytes,kWorkerBytes);
    if(g.workerReadable) {
        g.classMatch=read_value<std::uint32_t>(bytes+4)==kWorkerDefinitionClass;
        g.configuration=read_value<std::uint32_t>(bytes+0x96C);
        g.notBeyond=g.configuration!=beyond_forest::kConfiguration;
        g.notOmega=g.configuration!=omega_forest::kForestD;
        g.owner=read_value<std::uint32_t>(bytes+0x2C);
    }
    return g;
}

inline bool selected() noexcept { return evaluate(nullptr).selected(); }
inline bool worker(void* instance) noexcept {
    const auto g=evaluate(instance);
    return g.selected() && g.worker();
}

/**
 * FF4B20 skips replicated gateway creation without the worker owner's object-authority
 * bit, which is why the islands generate but the route never joins to the authored exit.
 * Omega repairs it in prepare_omega_forest and Beyond repairs it in its own prepare; both
 * are package-gated, so strike_bond never got it. Repair only this live owner's bit, and
 * only before native state 4 - the whole generation completes in about 125 ms. Gate
 * position and progress stay native.
 */
inline void prepare(void* instance) noexcept {
    const auto g=evaluate(instance);
    const bool pass=g.selected() && g.worker();
    bool repaired{},bitAlreadySet{},setterMissing{};
    if(pass) {
        const auto setter=g_forestOwnerAuthoritySetter.load(std::memory_order_acquire);
        const auto image=reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr));
        setterMissing=setter==nullptr || image==0 || g.owner==UINT32_MAX;
        if(!setterMissing) {
            const auto* word=reinterpret_cast<const std::byte*>(
                image+kObjectAuthorityTableRva+((g.owner&0x1FFFU)>>5U)*4U);
            if(readable(word,4)) {
                if((read_value<std::uint32_t>(word)&(1U<<(g.owner&31U)))==0U) { setter(g.owner,1U);repaired=true; }
                else { bitAlreadySet=true; }
            } else { setterMissing=true; }
        }
    }
    static std::atomic_uint32_t reports{};
    if(reports.fetch_add(1U,std::memory_order_relaxed)<24U) {
        std::array<char,320> line{};
        const int length=std::snprintf(line.data(),line.size(),
            "ev=forest stage=garden_gate pass=%u override=%u pkg=%u pkg_name=%.*s seed=%u world=%u "
            "owner_valid=%u enabled=%u not_finished=%u readable=%u class=%u not_beyond=%u not_omega=%u "
            "config=0x%08X expected=0x%08X owner=0x%08X repaired=%u already_set=%u setter_missing=%u",
            pass?1U:0U,g.overrideActive?1U:0U,g.packageMatch?1U:0U,
            static_cast<int>(g.packageLength),g.package.data(),
            g.seedArmed?1U:0U,g.worldLive?1U:0U,g.ownerValid?1U:0U,g.frameEnabled?1U:0U,
            g.notFinished?1U:0U,g.workerReadable?1U:0U,g.classMatch?1U:0U,g.notBeyond?1U:0U,
            g.notOmega?1U:0U,g.configuration,kForestC,g.owner,
            repaired?1U:0U,bitAlreadySet?1U:0U,setterMissing?1U:0U);
        if(length>0 && static_cast<std::size_t>(length)<line.size()) {
            core::log::write(core::log::Channel::client,core::log::Level::info,
                             {line.data(),static_cast<std::size_t>(length)});
        }
    }
}
}
