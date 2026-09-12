// Included by omega_reveal_native.cpp inside its private namespace. All pointers
// below are scoped to a native callback; only full handles survive between ticks.
bool copy_native(const void* source, void* target, std::size_t bytes) noexcept {
    if (!readable(source, bytes)) return false;
    __try { std::memcpy(target, source, bytes); return true; }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
template<class T> bool copy_value(const std::byte* address, T& value) noexcept {
    return copy_native(address, &value, sizeof value);
}
std::byte* resolve_handle(std::uint32_t handle) noexcept {
    if (handle == UINT32_MAX) return nullptr;
    const std::byte* registry{};
    const std::byte* tables{};
    if (!copy_value(image + 0x2439C70, registry) || !registry || !copy_value(registry, tables) || !tables)
        return nullptr;
    const auto shifted = static_cast<std::uint32_t>(static_cast<std::int32_t>(handle) >> 13);
    const auto bucket = ((std::uint64_t{shifted} | 0xFFC0000ULL) >> 18) & (shifted & 0xFFFFU);
    std::array<std::byte, 0x38> table{};
    if (!copy_native(tables + bucket * 0x40, table.data(), table.size())) return nullptr;
    const auto stride = read<std::int32_t>(table.data(), 0x30);
    const auto elements = read<std::uintptr_t>(table.data(), 8);
    if (stride <= 0 || stride > 0x2000000 || !elements) return nullptr;
    const auto offset = std::uintptr_t{handle & 0x1FFF} * static_cast<unsigned>(stride);
    if (elements > UINTPTR_MAX - offset - 16) return nullptr;
    const auto row = elements + offset;
    std::uint64_t relocation{};
    if (!copy_value(reinterpret_cast<const std::byte*>(row + 8), relocation)) return nullptr;
    relocation &= static_cast<std::uint64_t>(static_cast<std::int64_t>(read<std::int32_t>(table.data(), 0x34)));
    // Native C70180/DCBF30/F62F80 use a wrapping SUB here. The masked
    // relocation is a signed row-relative displacement, not an absolute
    // allocation address: a negative value means the datum is ABOVE its row.
    // The old unsigned comparison rejected that legal representation. Follow
    // the native subtraction and let every caller validate readable bytes and
    // full component identities before dereferencing the resulting address.
    return reinterpret_cast<std::byte*>(row - static_cast<std::uintptr_t>(relocation));
}
struct MemberView {
    std::uint32_t self{UINT32_MAX}, actor{UINT32_MAX}, generation{}, revision{}, controlRevision{};
    bool armControl{},armDomain{}; float leftControl{},rightControl{};
    std::int64_t offset{};
    std::int32_t head{}, count{};
    bool enabled{}, exactQueue{};
};
bool owner_wait(std::uint64_t run, unsigned stage, const char* field,
                std::uint64_t actual, std::uint64_t expected) noexcept {
    const std::unique_lock lock(mutex, std::try_to_lock);
    if (lock.owns_lock()) {
        reset(run);
        const auto bit = std::uint64_t{1} << stage;
        if (!(runState.ownerWaits & bit)) {
            runState.ownerWaits |= bit;
            log("ev=omega_reveal stage=owner_guard run=%llu check=%s actual=%016llX expected=%016llX accepted=0",
                run, field, static_cast<unsigned long long>(actual), static_cast<unsigned long long>(expected));
        }
    }
    return false;
}
bool member_view(std::byte* component, MemberView& out) noexcept {
    const frame_timing::PostSpan workTiming(frame_timing::Kind::member_view);
    std::array<std::byte, 0xAB4> member{};
    if (!copy_native(component, member.data(), member.size())
        || !omega_reveal_source::matches(member, {0x80F4756DU, 0x80807D9DU, 0xB58})) return false;
    // AB5080/AB6600 construct {owning datum handle, relative component offset}.
    // +24 is not a self handle on an activity member.
    native<std::uint32_t*(__fastcall*)(const std::byte*, std::uint32_t*) noexcept>(0x4E5C60)(component, &out.self);
    const auto datum = reinterpret_cast<std::uintptr_t>(resolve_handle(out.self));
    const auto address = reinterpret_cast<std::uintptr_t>(component);
    if (!datum || address < datum || address - datum > 0x2000000) return false;
    out.offset = static_cast<std::int64_t>(address - datum);
    out.actor = read<std::uint32_t>(member.data(), 0x21C);
    out.revision = read<std::uint32_t>(member.data(), 0x190);
    out.head = read<std::int32_t>(member.data(), 0x228);
    out.count = read<std::int32_t>(member.data(), 0x230);
    if (out.self == UINT32_MAX || out.count < 0 || out.count > 32
        || read<std::uint32_t>(member.data(), 0x220) != out.self
        || read<std::uint32_t>(member.data(), 0x224) != out.actor)
        return false;
    using Auth = const std::byte*(__fastcall*)(const std::byte*) noexcept;
    const std::byte* authority{};
    {const frame_timing::PostSpan timer(frame_timing::Kind::member_auth);
     authority = native<Auth>(0xAB27C0)(component);}
    std::array<std::byte, 0x108> auth{};
    if (!authority || !copy_native(authority, auth.data(), auth.size())) return false;
    out.generation = read<std::uint32_t>(auth.data(), 0);
    // AB71E0 retires the old actor before committing this generation at +180.
    // A newer authority body alone cannot relabel an older actor's graph.
    if (read<std::uint32_t>(member.data(), 0x180) != out.generation) return false;
    out.enabled = read<std::uint8_t>(auth.data(), 6) != 0
        && read<std::uint8_t>(member.data(), 0x1D4) == 0
        && read<std::uint32_t>(auth.data(), 0x100) == out.revision;
    // AB2D00 commits .5 at+AB0 before applying its scalar rows. Match both
    // revisions and the exact two-row domain; the caller also reads live scalars.
    out.controlRevision=read<std::uint32_t>(member.data(),0xAB0);
    out.armDomain=read<std::uint8_t>(auth.data(),0x50)==0 && read<std::uint8_t>(auth.data(),0x51)==0
        && read<std::uint32_t>(auth.data(),0x54)==0 && read<std::uint32_t>(auth.data(),0x70)==0x811C9DC5U
        && read<std::int8_t>(auth.data(),0x74)==-1 && read<std::int16_t>(auth.data(),0x76)==-1
        && read<std::int32_t>(auth.data(),0x78)==-1;
    for(unsigned i=0;i<6;++i) out.armDomain=out.armDomain && read<std::uint32_t>(auth.data(),0x58+i*4)==0x811C9DC5U;
    // AB7350 applies the default control on activation. Require its cached hash
    // list to agree before publishing, so a foreign native target list is not reset.
    for(unsigned i=0;i<7;++i) out.armDomain=out.armDomain
        && read<std::uint32_t>(member.data(),0xA90+i*4)==read<std::uint32_t>(auth.data(),0x54+i*4);
    out.armControl=out.armDomain && out.controlRevision!=0 && read<std::uint32_t>(auth.data(),0x4C)==out.controlRevision
        && read<std::uint32_t>(auth.data(),0x7C)==2
        && read<std::uint32_t>(auth.data(),0x80)==0xA2AE120FU
        && read<std::uint32_t>(auth.data(),0x88)==0x8496ABD2U;
    out.leftControl=read<float>(auth.data(),0x84);out.rightControl=read<float>(auth.data(),0x8C);
    graph::Queue queue{};
    std::memcpy(queue.data(), member.data() + 0x230, queue.size());
    out.exactQueue = graph::queue_matches(queue);
    return true;
}
bool character_owner(const MemberView& member, std::uint64_t run, std::uint32_t character,
                     graph::Owner& owner) noexcept {
    const frame_timing::PostSpan workTiming(frame_timing::Kind::character_owner);
    std::array<std::byte, 0x5C8> body{};
    auto* actualCharacter = resolve_handle(character);
    if (!actualCharacter || !copy_native(actualCharacter, body.data(), body.size()))
        return owner_wait(run, 0, "character_read", character, reinterpret_cast<std::uintptr_t>(actualCharacter));
    if (read<std::uint32_t>(body.data(), 0) != 0x80F6690BU)
        return owner_wait(run, 1, "character_resource", read<std::uint32_t>(body.data(), 0), 0x80F6690BU);
    if (read<std::uint32_t>(body.data(), 4) != 0x80806832U)
        return owner_wait(run, 2, "character_class", read<std::uint32_t>(body.data(), 4), 0x80806832U);
    if (read<std::int64_t>(body.data(), 8) != 0x738)
        return owner_wait(run, 3, "character_offset", read<std::uint64_t>(body.data(), 8), 0x738);
    if (read<std::uint32_t>(body.data(), 0x24) != character)
        return owner_wait(run, 4, "character_self", read<std::uint32_t>(body.data(), 0x24), character);
    if (read<std::uint32_t>(body.data(), 0xC0) != member.actor)
        return owner_wait(run, 5, "character_actor", read<std::uint32_t>(body.data(), 0xC0), member.actor);
    const auto animation = read<std::uint32_t>(body.data(), 0x5C0);
    auto* actualAnimation = resolve_handle(animation);
    std::array<std::byte, 0xC8> state{};
    if (!actualAnimation || !copy_native(actualAnimation, state.data(), state.size()))
        return owner_wait(run, 6, "animation_read", animation, reinterpret_cast<std::uintptr_t>(actualAnimation));
    if (read<std::uint32_t>(state.data(), 4) != 0x80F4519AU)
        return owner_wait(run, 7, "animation_config", read<std::uint32_t>(state.data(), 4), 0x80F4519AU);
    if (read<std::uint32_t>(state.data(), 0x30) != character)
        return owner_wait(run, 8, "animation_character", read<std::uint32_t>(state.data(), 0x30), character);
    if (read<std::uint32_t>(state.data(), 0xC4) != member.actor)
        return owner_wait(run, 9, "animation_actor", read<std::uint32_t>(state.data(), 0xC4), member.actor);
    std::array<std::byte, 24> parentContext{};
    {const frame_timing::PostSpan timer(frame_timing::Kind::parent_context);
    native<void(__fastcall*)(void*, std::uint32_t) noexcept>(0xA8CB20)(parentContext.data(), member.actor);}
    const auto parent = read<std::uint32_t>(parentContext.data(), 0);
    if (parent == UINT32_MAX)
        return owner_wait(run, 10, "parent_lookup", parent, member.actor);
    if (read<std::uint32_t>(state.data(), 0xC0) != parent)
        return owner_wait(run, 11, "animation_parent", read<std::uint32_t>(state.data(), 0xC0), parent);
    if (!readable(resolve_handle(parent), 16))
        return owner_wait(run, 12, "parent_read", parent, 1);
    // DCBF30 indexes the world-entity pool, never the AI actor pool. Original
    // F51CF0/DD18E0 pass component+2C. Character+C0 stores the separate AI actor.
    const auto entity = read<std::uint32_t>(body.data(), 0x2C);
    if (entity == UINT32_MAX)
        return owner_wait(run, 28, "character_entity", entity, member.actor);
    std::uint32_t biped = UINT32_MAX;
    LARGE_INTEGER lookupBegin{},lookupEnd{},lookupFrequency{};
    QueryPerformanceCounter(&lookupBegin);
    {const frame_timing::PostSpan timer(frame_timing::Kind::biped_lookup);
    native<std::uint32_t*(__fastcall*)(std::uint32_t*, std::uint32_t) noexcept>(0xDCBF30)(&biped, entity);}
    QueryPerformanceCounter(&lookupEnd);
    if(QueryPerformanceFrequency(&lookupFrequency) && lookupFrequency.QuadPart>0) {
        const double elapsedMs=1000.0*static_cast<double>(lookupEnd.QuadPart-lookupBegin.QuadPart)
            /static_cast<double>(lookupFrequency.QuadPart);
        if(elapsedMs>=2.0) {
            const std::unique_lock lock(mutex,std::try_to_lock);
            if(lock.owns_lock() && runState.run==run) {
                const auto phase=static_cast<unsigned>(runState.graphPhase);
                const auto bit=UINT64_C(1)<<(32+(phase&15U));
                if(!(runState.ownerWaits&bit)) {
                    runState.ownerWaits|=bit;
                    log("ev=omega_owner_timing run=%llu graph_phase=%u native=DCBF30 elapsed_ms=%.3f source=owned_biped_lookup",run,phase,elapsedMs);
                }
            }
        }
    }
    auto* actualBiped = resolve_handle(biped);
    std::array<std::byte, 0xD4> bipedBody{};
    if (!actualBiped || !copy_native(actualBiped, bipedBody.data(), bipedBody.size()))
        return owner_wait(run, 13, "biped_read", biped, reinterpret_cast<std::uintptr_t>(actualBiped));
    if (read<std::uint32_t>(bipedBody.data(), 0) != 0x80F66907U)
        return owner_wait(run, 14, "biped_resource", read<std::uint32_t>(bipedBody.data(), 0), 0x80F66907U);
    if (read<std::uint32_t>(bipedBody.data(), 4) != 0x808036CFU)
        return owner_wait(run, 15, "biped_class", read<std::uint32_t>(bipedBody.data(), 4), 0x808036CFU);
    if (read<std::int64_t>(bipedBody.data(), 8) != 0x1B48)
        return owner_wait(run, 16, "biped_offset", read<std::uint64_t>(bipedBody.data(), 8), 0x1B48);
    if (read<std::uint32_t>(bipedBody.data(), 0x24) != biped)
        return owner_wait(run, 17, "biped_self", read<std::uint32_t>(bipedBody.data(), 0x24), biped);
    if (read<std::uint32_t>(bipedBody.data(), 0x2C) != entity)
        return owner_wait(run, 29, "biped_entity", read<std::uint32_t>(bipedBody.data(), 0x2C), entity);
    if (read<std::uint32_t>(bipedBody.data(), 0xD0) != 0x80F45190U)
        return owner_wait(run, 18, "biped_bank", read<std::uint32_t>(bipedBody.data(), 0xD0), 0x80F45190U);
    owner.run = run; owner.member = member.self; owner.actor = member.actor;
    owner.generation = member.generation; owner.revision = member.revision;
    owner.parent = parent; owner.character = character; owner.animation = animation; owner.biped = biped;
    owner.entity = entity;
    owner.memberOffset = member.offset;
    return graph::valid_owner(owner) || owner_wait(run, 19, "distinct_owner_contract", character, parent);
}
bool event_table(const graph::Owner& owner, graph::EventTable& out) noexcept {
    auto* animation = resolve_handle(owner.animation);
    std::array<std::byte, 0x84> bytes{};
    return animation && copy_native(animation + 0x34, bytes.data(), bytes.size())
        && graph::parse_event_table(bytes, out);
}
void __fastcall character_initialize(std::byte* character, const std::uint32_t* actor) noexcept {
    const hooking::CallGate::Scope call(callGate);
    hooking::await_original(characterOriginal)(character, actor);
    if (!call.accepts_side_effects() || !active()) return;
    observe_mission_character(character,actor);
    std::array<std::byte, 0xC4> copy{};
    std::uint32_t fullActor{};
    if (!actor || !copy_native(actor, &fullActor, sizeof fullActor)
        || !copy_native(character, copy.data(), copy.size())
        || !omega_reveal_source::matches(copy, {0x80F6690BU, 0x80806832U, 0x738})
        || read<std::uint32_t>(copy.data(), 0xC0) != fullActor) return;
    const auto self = read<std::uint32_t>(copy.data(), 0x24);
    if (self == UINT32_MAX || fullActor == UINT32_MAX) return;
    observedCharacter.store((std::uint64_t{fullActor} << 32) | self, std::memory_order_release);
    const auto run = state::activity::mission_run_generation();
    const std::unique_lock lock(mutex, std::try_to_lock);
    if (!lock.owns_lock()) return;
    reset(run);
    if (runState.queueClaimed && runState.graphOwner.actor != fullActor) return;
    log("ev=omega_reveal stage=character_bound run=%llu actor=%08X character=%08X source=80F6690B/738", run, fullActor, self);
}

void member_wait(std::uint64_t run, unsigned bit, const char* reason) noexcept {
    const std::unique_lock lock(mutex, std::try_to_lock);
    if (!lock.owns_lock()) return;
    reset(run);
    if (runState.memberWaits & bit) return;
    runState.memberWaits |= bit;
    log("ev=omega_reveal stage=member_wait run=%llu reason=%s", run, reason);
}

bool current_owner(const graph::Owner& issued, MemberView& member, graph::Owner& current) noexcept {
    if (!graph::valid_owner(issued)) return false;
    auto* datum = resolve_handle(issued.member);
    if (!datum || !member_view(datum + issued.memberOffset, member)
        || member.self != issued.member || member.offset != issued.memberOffset) return false;
    return character_owner(member, issued.run, issued.character, current);
}

bool named_pair(const graph::Owner& owner) noexcept {
    auto* config = resolve_handle(0x80F4519AU);
    std::array<std::byte, 0x1C4> configBytes{};
    if (!config || !copy_native(config, configBytes.data(), configBytes.size()))
        return owner_wait(owner.run, 20, "named_config_read", 0x80F4519AU, reinterpret_cast<std::uintptr_t>(config));
    if (!graph::named_config_layout(configBytes))
        return owner_wait(owner.run, 24, "named_config_layout", read<std::uint64_t>(configBytes.data(), 0x20), 2);
    std::int32_t group = -1, sequence = -1;
    using Lookup = bool(__fastcall*)(const void*, const std::uint32_t*, const std::uint32_t*,
                                    std::int32_t*, std::int32_t*) noexcept;
    // This is the same actual configuration reached through character+5C0.
    if (!graph::valid_owner(owner)) return false;
    if (!native<Lookup>(0xA889E0)(config, &graph::kGroup, &graph::kSequence, &group, &sequence))
        return owner_wait(owner.run, 21, "named_lookup", static_cast<std::uint32_t>(group), static_cast<std::uint32_t>(sequence));
    if (group != 0) return owner_wait(owner.run, 22, "named_group", static_cast<std::uint32_t>(group), 0);
    if (sequence != 1) return owner_wait(owner.run, 23, "named_sequence", static_cast<std::uint32_t>(sequence), 1);
    return true;
}

void retire_queue(const graph::Owner& issued, const graph::Owner& current) noexcept {
    graph::EventTable before{};
    const bool readableTable = graph::same_binding(issued, current) && event_table(current, before);
    bool remove{};
    {
        const std::unique_lock lock(mutex, std::try_to_lock);
        if (!lock.owns_lock() || runState.run != issued.run || runState.graphOwner != issued) return;
        if (!runState.graphRetired) {
            runState.graphRetired = true;
            log("ev=omega_reveal stage=graph_retired run=%llu actor=%08X reason=owned_queue_no_longer_active", issued.run, issued.actor);
        }
        remove = readableTable && runState.summonLease.begin_remove(current, before);
    }
    if (!remove) return;
    const auto request = graph::make_condition_request();
    native<void(__fastcall*)(std::byte*, const void*, const void*) noexcept>(0xC693F0)
        (resolve_handle(current.character), request.data(), nullptr);
    MemberView afterMember{};
    graph::Owner afterOwner{};
    graph::EventTable after{};
    const bool confirmed = current_owner(current, afterMember, afterOwner) && afterOwner == current
        && event_table(afterOwner, after);
    const std::lock_guard lock(mutex);
    if (runState.run != issued.run || runState.graphOwner != issued) return;
    const bool removed = runState.summonLease.finish_remove(afterOwner, after, confirmed);
    log("ev=omega_reveal stage=summon_condition_remove run=%llu actor=%08X confirmed=%u before=%d after=%d",
        issued.run, issued.actor, removed ? 1U : 0U, graph::reference_count(before),
        confirmed ? graph::reference_count(after) : -1);
}

// AB6600 reconciles authority messages; it is not guaranteed to run after the
// player approaches. The owned graph callback also pumps this condition after
// F4E660 returns, when its event-table traversal has finished.
void pump_summon(const graph::Owner& issued, const hooking::CallGate::Scope& call) noexcept {
    const auto run = state::activity::mission_run_generation();
    if (run != issued.run || !call.accepts_side_effects() || !active()
        || omega::presentation_progress(run).route != omega::Route::reveal) return;
    {
        const std::unique_lock lock(mutex, std::try_to_lock);
        if (!lock.owns_lock() || runState.run != run || runState.graphOwner != issued
            || !runState.queueAccepted || runState.graphRetired
            || runState.graphPhase != observation::Phase::hover) return;
        if (runState.summonLease.state() != graph::LeaseState::idle) {
            if (runState.summonLease.state() != graph::LeaseState::owned && !(runState.memberWaits & 128U)) {
                runState.memberWaits |= 128U;
                log("ev=omega_reveal stage=summon_wait run=%llu reason=lease_not_idle lease=%u",
                    run, static_cast<unsigned>(runState.summonLease.state()));
            }
            return;
        }
    }
    MemberView member{};
    graph::Owner owner{};
    if (!current_owner(issued, member, owner) || owner != issued || !member.enabled
        || member.head != 0 || !member.exactQueue) {
        member_wait(run, 256, "summon_owner_or_queue_no_longer_current"); return;
    }
    graph::EventTable before{};
    if (!event_table(owner, before)) { member_wait(run, 64, "condition_table_unreadable"); return; }
    {
        const std::unique_lock lock(mutex, std::try_to_lock);
        if (!lock.owns_lock() || runState.run != run || runState.graphOwner != owner
            || runState.graphRetired || runState.graphPhase != observation::Phase::hover
            || !call.accepts_side_effects()) return;
        if (!runState.summonLease.begin_add(owner, before, true, true)) {
            if (!(runState.memberWaits & 512U)) {
                runState.memberWaits |= 512U;
                log("ev=omega_reveal stage=summon_wait run=%llu reason=condition_lease_preflight lease=%u count=%d refs=%d",
                    run, static_cast<unsigned>(runState.summonLease.state()), before.count, graph::reference_count(before));
            }
            return;
        }
    }
    // Only the named condition reference changes. No graph state is written;
    // the next native graph evaluation chooses its authored summon transition.
    const auto request = graph::make_condition_request();
    native<void(__fastcall*)(std::byte*, const void*, const void*) noexcept>(0xC620F0)
        (resolve_handle(owner.character), request.data(), nullptr);
    MemberView afterMember{};
    graph::Owner afterOwner{};
    graph::EventTable after{};
    const bool confirmed = current_owner(owner, afterMember, afterOwner) && afterOwner == owner
        && afterMember.enabled && afterMember.head == 0 && afterMember.exactQueue
        && event_table(afterOwner, after);
    const std::lock_guard lock(mutex);
    if (runState.run != run || runState.graphOwner != owner) return;
    const bool added = runState.summonLease.finish_add(afterOwner, after, confirmed);
    log("ev=omega_reveal stage=summon_condition run=%llu actor=%08X confirmed=%u before=%d after=%d "
        "event=C0F9C866 trigger=closer_approach_and_native_hover", run, owner.actor, added ? 1U : 0U,
        graph::reference_count(before), confirmed ? graph::reference_count(after) : -1);
}

void __fastcall member_tick(std::byte* component) noexcept {
    const hooking::CallGate::Scope call(callGate);
    hooking::await_original(memberOriginal)(component);
    if (!call.accepts_side_effects() || !active()) return;
    std::array<std::byte, 16> prefix{};
    if (!copy_native(component, prefix.data(), prefix.size())
        || !omega_reveal_source::matches(prefix, {0x80F4756DU, 0x80807D9DU, 0xB58})) return;
    const auto run = state::activity::mission_run_generation();
    const auto progress = omega::presentation_progress(run);
    MemberView member{};
    if (!member_view(component, member)) { member_wait(run, 1, "member_binding_not_ready"); return; }
    graph::Owner issued{};
    bool claimed{}, accepted{}, retired{};
    std::uint32_t character = UINT32_MAX;
    {
        const std::unique_lock lock(mutex, std::try_to_lock);
        if (!lock.owns_lock()) return;
        reset(run);
        claimed = runState.queueClaimed; accepted = runState.queueAccepted; retired = runState.graphRetired;
        issued = runState.graphOwner;
        const auto observed = observedCharacter.load(std::memory_order_acquire);
        if (static_cast<std::uint32_t>(observed >> 32) == member.actor)
            character = static_cast<std::uint32_t>(observed);
    }
    graph::Owner owner{};
    if (claimed) {
        // A new actor, datum generation or sibling member never inherits this command.
        if (!accepted || retired || !character_owner(member, run, issued.character, owner)) return;
        if (owner != issued || !member.enabled || !member.exactQueue || member.head != 0) {
            retire_queue(issued, owner);
            return;
        }
    } else {
        if (!omega_boss_spawn::eligible(true, progress)) return;
        if (!member.enabled || member.generation != omega::boss_generation(run, progress)
            || member.actor == UINT32_MAX) { member_wait(run, 2, "awaiting_member_authority_actor"); return; }
        if (character == UINT32_MAX) { member_wait(run, 4, "awaiting_character_initializer"); return; }
        if (!character_owner(member, run, character, owner) || !named_pair(owner)) {
            member_wait(run, 8, "character_animation_binding_not_ready"); return;
        }
        if (member.count != 0 && member.head < member.count) {
            member_wait(run, 16, "existing_native_command_active"); return;
        }
        const std::uint32_t cinematic = 0xA74B2200U;
        if (!native<void*(__fastcall*)(const std::uint32_t*)>(0xC4C1A0)(&cinematic)) {
            member_wait(run, 32, "cinematic_not_registered"); return;
        }
        // Visual initialization is a candidate; an unavailable read must not
        // suppress the working intro. Native side effects can invalidate owners,
        // so independently validate the complete chain and queue afterward.
        if (!prepare_intro_vfx(owner, call)) member_wait(run, 1024, "vfx_initialization_unconfirmed");
        MemberView freshMember{};
        graph::Owner freshOwner{};
        if (!call.accepts_side_effects() || state::activity::mission_run_generation() != run
            || !current_owner(owner, freshMember, freshOwner) || freshOwner != owner
            || !freshMember.enabled || (freshMember.count != 0 && freshMember.head < freshMember.count)) {
            member_wait(run, 2048, "owner_or_queue_changed_before_intro"); return;
        }
        {
            const std::unique_lock lock(mutex, std::try_to_lock);
            if (!lock.owns_lock() || runState.run != run || runState.queueClaimed) return;
            // Reserve before entering native code; an uncertain call is never retried.
            runState.queueClaimed = true; runState.graphOwner = owner;
            runState.bossSeen = true;
        }
        const auto queue = graph::make_queue();
        native<void(__fastcall*)(std::byte*, const void*, std::int32_t) noexcept>(0xAB6C60)
            (component, queue.data(), graph::kQueueHead);
        MemberView afterMember{};
        graph::Owner afterOwner{};
        const bool confirmed = current_owner(owner, afterMember, afterOwner) && afterOwner == owner
            && afterMember.enabled && afterMember.head == 0 && afterMember.exactQueue;
        const std::lock_guard lock(mutex);
        if (runState.run != run || runState.graphOwner != owner) return;
        runState.queueAccepted = confirmed;
        log("ev=omega_reveal stage=graph_queue run=%llu member=%08X offset=%llX actor=%08X character=%08X "
            "animation=%08X biped=%08X entity=%08X generation=%u revision=%u confirmed=%u group=AFB11A12 sequence=65D2379F",
            run, owner.member, static_cast<unsigned long long>(owner.memberOffset), owner.actor,
            owner.character, owner.animation, owner.biped, owner.entity, owner.generation, owner.revision, confirmed ? 1U : 0U);
        return;
    }
    pump_summon(owner, call);
}

bool __fastcall graph_update(float dt, const void* context, std::byte* state, bool* transitioned) noexcept {
    const hooking::CallGate::Scope call(callGate);
    const bool result = [&] {const frame_timing::PostSpan timer(frame_timing::Kind::graph_native);
        return hooking::await_original(graphOriginal)(dt, context, state, transitioned);}();
    const frame_timing::PostSpan postTiming(frame_timing::Kind::graph);
    if (!call.accepts_side_effects() || !active()) return result;
    std::array<std::byte, observation::kGraphBytes> graphBytes{};
    if (!copy_native(state, graphBytes.data(), graphBytes.size())
        || read<std::uint32_t>(graphBytes.data(), 0x10) != observation::kGraphAsset) return result;
    observe_mission_crown(graphBytes,result,call);
    observe_eye_clip_cursor(context,graphBytes,call);
    const auto run = state::activity::mission_run_generation();
    graph::Owner issued{};
    {
        const std::unique_lock lock(mutex, std::try_to_lock);
        if (!lock.owns_lock() || runState.run != run || !runState.queueAccepted || runState.graphRetired)
            return result;
        issued = runState.graphOwner;
    }
    if (read<std::uint32_t>(graphBytes.data(), 0) != issued.entity
        || read<std::uint32_t>(graphBytes.data(), 4) != issued.character
        || read<std::uint32_t>(graphBytes.data(), 0x14) != issued.biped) {
        const std::unique_lock lock(mutex, std::try_to_lock);
        if (lock.owns_lock() && runState.run == run && runState.graphRejectSamples++ < 8)
            log("ev=omega_reveal stage=graph_owner_rejected run=%llu entity=%08X/%08X character=%08X/%08X biped=%08X/%08X",
                run, read<std::uint32_t>(graphBytes.data(), 0), issued.entity,
                read<std::uint32_t>(graphBytes.data(), 4), issued.character,
                read<std::uint32_t>(graphBytes.data(), 0x14), issued.biped);
        return result;
    }
    MemberView member{};
    graph::Owner current{};
    if (!current_owner(issued, member, current)) return result;
    if (current != issued || !member.enabled || member.head != 0 || !member.exactQueue) {
        if (graph::same_binding(issued, current)) retire_queue(issued, current);
        return result;
    }
    std::array<std::byte, 0x440> bank{};
    if (!copy_native(resolve_handle(observation::kBankAsset), bank.data(), bank.size())) return result;
    observation::Snapshot snapshot{};
    const bool decoded = observation::decode(graphBytes, bank, result, snapshot)
        && observation::belongs_to(snapshot, current, issued, graph::kGroup, graph::kSequence);
    if (decoded) observe_eye_inputs(issued, snapshot);
    std::unique_lock lock(mutex, std::try_to_lock);
    if (!lock.owns_lock() || runState.run != run || runState.graphOwner != issued) return result;
    if (!decoded) {
        if (runState.graphRejectSamples++ < 8)
            log("ev=omega_reveal stage=graph_observation_rejected run=%llu result=%u node=%d requested=%d "
                "bank_row=%d active=%u time=%.4f limit=%.4f", run, result ? 1U : 0U, snapshot.loadedNode,
                snapshot.requestedNode, snapshot.bankRow, snapshot.active ? 1U : 0U,
                static_cast<double>(snapshot.playbackTime), static_cast<double>(snapshot.playbackLimit));
        return result;
    }
    if (snapshot.phase == observation::Phase::fly) runState.flightSeen = true;
    if (snapshot.phase == observation::Phase::summon
        && runState.summonLease.state() == graph::LeaseState::owned)
        runState.summonSeen = true;
    if (snapshot.phase == observation::Phase::idle && runState.summonSeen)
        runState.introIdleSeen = true;
    if (runState.graphPhase != snapshot.phase) {
        runState.graphPhase = snapshot.phase;
        if (callGate.accepting())
            frame_timing::set_scope(run, static_cast<std::uint32_t>(snapshot.phase));
        log("ev=omega_reveal stage=graph_node run=%llu actor=%08X entity=%08X character=%08X biped=%08X node=%d "
            "phase=%u clip=%08X bank_row=%d time=%.4f loop=%u event_owner=native_graph",
            run, issued.actor, snapshot.entity, snapshot.character, snapshot.biped, snapshot.loadedNode,
            static_cast<unsigned>(snapshot.phase), snapshot.clip, snapshot.bankRow,
            static_cast<double>(snapshot.playbackTime), snapshot.loop ? 1U : 0U);
    }
    lock.unlock();
    if (snapshot.phase == observation::Phase::hover) pump_summon(issued, call);
    return result;
}
