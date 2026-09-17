# Omega: teardown into the ending cutscene

Source snapshot: 8 September 2026, workspace HEAD `880c2b760ae923c54afd9706af6b1d537dcad5d4`.

Omega ends by publishing explicit removals for the Lair roster, observing native removal finish while the player is still in the Lair, and then enabling the Lighthouse bookend state and host teleport. The ending cinematic starts only after native arrival and registered-camera readiness.

This documents the `omega_ending` / `coo::ending::Controller` path connected to the Lua ending graph, roster publisher, native retirement hook, and Mercury handoff. Code blocks are verbatim workspace excerpts with their existing project dependencies; they are not standalone replacement code. The separate `omega::ending::runtime` implementation under `src/state/activity/omega/` uses a different authority path and is not interchangeable with these snippets. No installed DLL or live playthrough was checked for this document.

## 1. The order

1. The third Crown cycle reaches its qualified ending state and supplies the encounter's run, epoch, actor, and generation.
2. Osiris's final gameplay dialogue is submitted and its playback window finishes.
3. Publish explicit zero-presence entries for the old streamed roster keys.
4. Native roster apply removes the Lair groups. Validate the same owner before and after that call.
5. Accept `lair.retired`; the controller can now request the bookend state and Lighthouse teleport.
6. Observe arrival in region 121 for the exact host teleport token and spawn set.
7. Observe the registered ending camera ready and inactive; publish a new play revision.
8. Observe native active playback at that revision, then native inactive playback at the current revision to complete.
9. Queue the Mercury activity handoff.

The Lua graph states the sequence and receipt bindings:

Source: [Dawn/scripts/omega.lua:436](<C:/Destiny 2 Development/Dawn/scripts/omega.lua:436>) (lines 436-468).

```lua
local ending = graph("ending", "Omega ending and handoff", sequence(
    step("final dialogue", native("ending/final dialogue/0", "final_dialogue.observation.0")),
    step("native Lair retirement", parallel(
        native("ending/native Lair retirement/0", "native_lair_retirement.mechanic.0"),
        native("ending/native Lair retirement/1", "native_lair_retirement.observation.1")
    )),
    step("bookend arrival and native camera eligibility", parallel(
        native("ending/bookend arrival and native camera eligibility/0", "bookend_arrival_and_native_camera_eligibility.traversal.0"),
        native("ending/bookend arrival and native camera eligibility/1", "bookend_arrival_and_native_camera_eligibility.observation.1")
    )),
    step("native cinematic activation", parallel(
        native("ending/native cinematic activation/0", "native_cinematic_activation.cinematic.0"),
        native("ending/native cinematic activation/1", "native_cinematic_activation.observation.1")
    )),
    step("native cinematic completion", parallel(
        native("ending/native cinematic completion/0", "native_cinematic_completion.cinematic.0"),
        native("ending/native cinematic completion/1", "native_cinematic_completion.observation.1")
    )),
    step("retire cinematic command", native("ending/retire cinematic command/0", "retire_cinematic_command.cinematic.0")),
    step("native Mercury launch queued", parallel(
        native("ending/native Mercury launch queued/0", "native_mercury_launch_queued.traversal.0"),
        native("ending/native Mercury launch queued/1", "native_mercury_launch_queued.observation.1")
    ))
), {
    receipts={
        ["dialogue.complete"]="final_dialogue.observation.0",
        ["lair.retired"]="native_lair_retirement.observation.1",
        ["camera.eligible"]="bookend_arrival_and_native_camera_eligibility.observation.1",
        ["camera.active"]="native_cinematic_activation.observation.1",
        ["camera.complete"]="native_cinematic_completion.observation.1",
        ["handoff.queued"]="native_mercury_launch_queued.observation.1",
    },
})
```

## 2. Entry and final dialogue

`NativeControllers::request_ending()` in `coo/omega_adapter.cpp` passes the encounter token to `omega_ending::request()`. That function reacquires the encounter status, requires cycle 3 and `CrownStage::ending`, validates the immutable owner identity, and claims `finishEncounter` when appropriate.

The presentation gate prevents the boss death alone from cutting off the last line:

Source: [Dawn/src/state/activity/omega_presentation_rules.h:92](<C:/Destiny 2 Development/Dawn/src/state/activity/omega_presentation_rules.h:92>) (lines 92-95).

```cpp
    [[nodiscard]] bool ending_dialogue_finished(std::uint64_t now) const noexcept {
        return defeated_ && finalDialogueSubmitted_ && now >= dialogue_.voice_until()
            && presentation_.activeRow == kNoDialogue;
    }
```

`omega_ending::authority()` reads that gate, then calls `Controller::advance()`. In executor mode, completion emits `dialogue.complete` and starts the retirement step.

These cases are inside `Controller::Driver::request()`. `retire` requests removals; only the later `arrive` command sets `stateRequested_`, which becomes `Authority::bookendState`:

Source: [Dawn/src/state/activity/coo/omega_ending_controller.h:143](<C:/Destiny 2 Development/Dawn/src/state/activity/coo/omega_ending_controller.h:143>) (lines 143-162).

```cpp
            case Request::retire:
                if(spec.operation!=Operation::mechanic || spec.asset!=kRetirement) { return false; }
                owner_.phase_=native::Phase::retiring;owner_.retireRequested_=true;owner_.requestedAt_=owner_.now_;return true;
            case Request::arrive:
                if(spec.operation!=Operation::traversal || spec.asset!=kArrival) { return false; }
                owner_.stateRequested_=true;owner_.phase_=native::Phase::preparing;owner_.requestedAt_=owner_.now_;return true;
            case Request::play:
                if(spec.operation!=Operation::cinematic || spec.asset!=kBookend) { return false; }
                owner_.revision_=owner_.nextRevision_++;owner_.play_=true;owner_.phase_=native::Phase::offered;
                owner_.offeredAt_=owner_.now_;++owner_.attempts_;return true;
            case Request::active:
                if(spec.operation!=Operation::cinematic || spec.asset!=kBookend) { return false; }
                owner_.phase_=native::Phase::playing;owner_.started_=true;owner_.startedAt_=owner_.now_;return true;
            case Request::finish:
                if(spec.operation!=Operation::cinematic || spec.asset!=kBookend) { return false; }
                owner_.phase_=native::Phase::complete;owner_.play_=false;owner_.revision_=owner_.nextRevision_++;return true;
            case Request::handoff:
                if(spec.operation!=Operation::traversal || spec.asset!=kHandoff) { return false; }
                owner_.handoff_=native::Handoff::pending;return true;
            }
```

Hooks enqueue copied receipts under the ending lock. `advance()` drains them and drives the executor from publication or the game-frame owner. A successful receipt enqueue can therefore precede the visible phase change. The non-selected controller delegates to `omega_ending::Ending` in `omega_ending_rules.h`, which keeps the same retirement-before-transition gate.

## 3. Publish actual removals

`coo/omega_projection.h` maps `Authority::retireRoster` to `snapshot.omegaEndingRetire`. The roster builder responds by calling `omega_lair::terminal_roster()`.

Native roster apply at RVA `0x3CCE50` compares entries at the same bubble/key ordinal and walks the incoming count. To express removal, retain the old blocks and keys in order and change their presence to zero. Dropping a block or shortening it can leave old native groups alive.

The terminal roster preserves the three global service groups, zeroes the old streamed-key presence, and appends the ending registry to bubble 15 with presence one. The final group descriptor is type 6, slot 0.

Source: [Dawn/src/server/bap/encrypted/push/activity/omega_lair_roster.h:291](<C:/Destiny 2 Development/Dawn/src/server/bap/encrypted/push/activity/omega_lair_roster.h:291>) (lines 291-348).

```cpp
template<class Storage>
[[nodiscard]] bool terminal_roster(Storage& storage,wire::Roster& roster,
    std::uint8_t authoredState) noexcept {
    namespace ending=state::activity::omega_ending;
    if(authoredState!=ending::kState || roster.topLevelGroupCount!=3 || roster.groupCount<3
        || roster.groups[0].key!=0x4786C0E0U || roster.groups[1].key!=0x29D7B029U
        || roster.groups[2].key!=0x82FB58B7U) { return false; }
    const auto blocks=roster.bubbleSubBlocks;
    std::size_t endingBlock=blocks.size();
    bool hasEnding{};
    for(std::size_t i=0;i<blocks.size();++i) {
        const auto& block=blocks[i];
        if(i>=storage.rosterSubBlocks.size() || i>=storage.rosterSubBlockKeys.size()
            || block.keys.empty() || block.keys.size()>wire::kBubbleKeyCapacity
            || block.keys.size()>storage.rosterSubBlockKeys[i].size()) { return false; }
        for(std::size_t earlier=0;earlier<i;++earlier) {
            if(blocks[earlier].bubble==block.bubble) { return false; }
        }
        if(block.bubble==ending::kBubble) { endingBlock=i; }
        for(std::size_t key=0;key<block.keys.size();++key) {
            if(block.keys[key]!=ending::kRegistry) { continue; }
            // Our prior publication always appends this key exactly once.
            if(hasEnding || block.bubble!=ending::kBubble || key+1!=block.keys.size()) {
                return false;
            }
            hasEnding=true;
        }
    }
    if(endingBlock>=storage.rosterSubBlocks.size()
        || endingBlock>=storage.rosterSubBlockKeys.size()) { return false; }
    const auto endingCount=endingBlock<blocks.size()?blocks[endingBlock].keys.size():0U;
    const auto nextEndingCount=endingCount+(hasEnding?0U:1U);
    if(nextEndingCount>wire::kBubbleKeyCapacity
        || nextEndingCount>storage.rosterSubBlockKeys[endingBlock].size()) { return false; }
    for(std::size_t i=0;i<blocks.size();++i) {
        const auto block=blocks[i];
        auto& keys=storage.rosterSubBlockKeys[i];
        if(block.keys.data()!=keys.data()) {
            std::copy(block.keys.begin(),block.keys.end(),keys.begin());
        }
        storage.rosterSubBlocks[i]={block.bubble,std::span(keys).first(block.keys.size()),
            std::span(kRetiredKeyPresence).first(block.keys.size())};
    }
    auto& endingKeys=storage.rosterSubBlockKeys[endingBlock];
    if(!hasEnding) { endingKeys[endingCount]=ending::kRegistry; }
    storage.rosterSubBlocks[endingBlock]={ending::kBubble,
        std::span(endingKeys).first(nextEndingCount),
        std::span(kEndingKeyPresence).last(nextEndingCount)};
    roster.bubbleSubBlocks=std::span(storage.rosterSubBlocks).first(
        blocks.size()+(endingBlock==blocks.size()?1U:0U));
    auto& group=storage.rosterGroups[3];
    group={};group.registryKey=ending::kRegistry;group.objectTag=ending::kRegistryTag;
    group.slotCount=1;group.slotTypes[0]=6;group.slotFlags[0]=2;group.slotIndices[0]=0;
    roster.groups[3]={group.registryKey,std::span(group.slotTypes).first(1),
        std::span(group.slotFlags).first(1),std::span(group.slotIndices).first(1)};
    roster.groupCount=4;
    return true;
}
```

The retirement publication also disables opening-transition, portal-entry, Forest-generator, and Crown restriction requests. It preserves mission authority state and keeps the global groups' generation stable through the transition. The explicit retired entries remain present in subsequent publications.

## 4. Wait for native cleanup to finish

`roster_apply_hook()` wraps the original `0x3CCE50` call. It checks the retirement request and captures the owner before the call, invokes the original exactly once, and validates the resulting native state before acknowledging retirement.

Source: [Dawn/src/client/hooks/bootflow/omega_dialogue_dispatch_probe.cpp:2157](<C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/omega_dialogue_dispatch_probe.cpp:2157>) (lines 2157-2180).

```cpp
__declspec(noinline) void __fastcall roster_apply_hook(void* context,const void* delta) noexcept {
    const hooking::CallGate::Scope scope(g_rosterGate);
    const auto original=hooking::await_original(g_rosterOriginal);
    namespace ending=state::activity::omega_ending;
    ending::Token token{};
    omega_teardown_native::Retirement lease{};
    if(scope.accepts_side_effects()) {
        token=ending::retirement_request(state::activity::mission_run_generation());
        if(token.valid()) {
            lease=omega_teardown_native::begin_retirement(teardown_source(),
                reinterpret_cast<std::uintptr_t>(context),reinterpret_cast<std::uintptr_t>(delta),current_roster_bubble());
            if(!lease.valid()) { retirement_log("before_guard",token,UINT32_MAX,false); }
        }
    }
    original(context,delta);
    if(!scope.accepts_side_effects() || !lease.valid()) { return; }
    if(state::activity::mission_run_generation()!=token.run
        || ending::retirement_request(token.run)!=token
        || !omega_teardown_native::finish_retirement(teardown_source(),lease,current_roster_bubble())) {
        retirement_log("after_guard",token,lease.handle,false);return;
    }
    const bool accepted=ending::observe_retirement(token);
    retirement_log(accepted?"native_cleanup_complete":"token_changed",token,lease.handle,accepted);
}
```

The native checks require:

- Current bubble 14 before and after apply.
- An active owner for Omega scenario `0x80F47522`, with the same handle, parent, and identity.
- Exactly these seven Lair keys: `F4D0E0B2`, `95FB2E01`, `0040BF06`, `0040BF05`, `0040BF03`, `99BD2FEB`, `0040BF04`.
- An explicit same-ordinal presence change from `0x7F` to zero, with the seven per-key state bytes unchanged.
- After the original returns, the receiver's applied roster mirror contains the zero-presence block and all seven old group descriptors are absent.

The mirror is at `context + 8`; the roster serialization buffers are different storage. These checks authenticate what native cleanup did:

Source: [Dawn/src/client/hooks/bootflow/omega_teardown_native.h:173](<C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/omega_teardown_native.h:173>) (lines 173-210).

```cpp
[[nodiscard]] inline Retirement begin_retirement(const Source& source,std::uintptr_t context,
                                                std::uintptr_t delta,std::uint32_t bubble) noexcept {
    Retirement result{};
    if(bubble!=14 || !retirement_owner(source,context,result)
        || !lair_groups(source,result.owner,true)) { return {}; }
    std::int32_t before{},after{};
    if(!read(source,context+8+0x528,before) || !read(source,delta+0x528,after)
        || before<1 || before>64 || after<1 || after>64) { return {}; }
    unsigned matches{};
    for(std::int32_t i=0;i<after;++i) {
        const auto offset=0x52CU+static_cast<std::uintptr_t>(i)*kRosterBlockBytes;
        std::uint32_t id{};
        if(!read(source,delta+offset,id)) { return {}; }
        if(id!=14) { continue; }
        if(++matches!=1 || i>=before) { return {}; }
        RosterBlock oldBlock{},newBlock{};
        if(!read(source,context+8+offset,oldBlock) || !read(source,delta+offset,newBlock)
            || !lair_block(oldBlock,true) || !lair_block(newBlock,false)
            || oldBlock.words[0x194/4]!=newBlock.words[0x194/4]) { return {}; }
        const auto* oldStates=reinterpret_cast<const std::byte*>(oldBlock.words.data())+0x198;
        const auto* newStates=reinterpret_cast<const std::byte*>(newBlock.words.data())+0x198;
        for(std::size_t j=0;j<kLairKeys.size();++j) { if(oldStates[j]!=newStates[j]) { return {}; } }
        result.block=static_cast<std::uint32_t>(i);
    }
    return matches==1 && same_retirement_owner(source,result) ? result : Retirement{};
}
/** After original 3CCE50 has returned: its mirror must contain the removal and
 * every old group descriptor must actually be absent. No native cleanup is called. */
[[nodiscard]] inline bool finish_retirement(const Source& source,const Retirement& prior,
                                            std::uint32_t bubble) noexcept {
    if(bubble!=14 || !same_retirement_owner(source,prior)
        || !lair_groups(source,prior.owner,false)) { return false; }
    std::int32_t count{};RosterBlock block{};
    return read(source,prior.context+8+0x528,count) && count>0 && count<=64
        && prior.block<static_cast<std::uint32_t>(count)
        && read(source,prior.context+8+0x52C+prior.block*kRosterBlockBytes,block)
        && lair_block(block,false) && same_retirement_owner(source,prior);
}
```

A packet send, transport acknowledgement, or elapsed timeout cannot produce `lair.retired`. In executor mode the accepted retirement receipt is drained into `observed("lair.retired")`, which releases the next graph step.

### Guard repeated or stale unregisters

The index-free hook at RVA `0x4D7C00` first calls `can_unregister()`. It resolves the native directory and requires the full datum to agree with the head/tail and neighboring links. A reused slot with a stale serial is insufficient. If the check fails, the hook skips that unregister; otherwise it forwards the original.

Source: [Dawn/src/client/hooks/bootflow/omega_teardown_native.h:82](<C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/omega_teardown_native.h:82>) (lines 82-112).

```cpp
[[nodiscard]] inline bool can_unregister(const Source& source,std::uintptr_t list,
                                         std::uint32_t datum) noexcept {
    const auto object=resolve(source,datum);
    std::uint32_t key{};std::int32_t count{};
    if(object.address==0 || !read(source,object.address,key) || !read(source,list,count)
        || count<=0 || count>=0x10000) { return false; }
    std::uintptr_t group{};
    for(std::int32_t i=0;i<count;++i) {
        std::uintptr_t row{};std::uint32_t candidate{};
        if(!add(list,4U+static_cast<std::uint64_t>(i)*16U,row) || !read(source,row,candidate)) { return false; }
        if(candidate==key) { group=row; } // Native lookup retains the last matching key.
    }
    std::uintptr_t field{};std::uint32_t head{},tail{};
    if(group==0 || !add(group,4,field) || !read(source,field,head)
        || !add(group,8,field) || !read(source,field,tail)) { return false; }
    std::uint32_t previous{},next{};
    if(!add(object.address,0x74,field) || !read(source,field,previous)
        || !add(object.address,0x78,field) || !read(source,field,next)
        || previous==datum || next==datum
        || (previous!=UINT32_MAX && previous==next)
        || (previous==UINT32_MAX)!=(head==datum)
        || (next==UINT32_MAX)!=(tail==datum)) { return false; }
    const auto neighbor_matches=[&](std::uint32_t neighbor,std::uint32_t offset) noexcept {
        if(neighbor==UINT32_MAX) { return true; }
        const auto node=resolve(source,neighbor);
        std::uint32_t nodeKey{},back{};std::uintptr_t link{};
        return node.address!=0 && read(source,node.address,nodeKey) && nodeKey==key
            && add(node.address,offset,link) && read(source,link,back) && back==datum;
    };
    return neighbor_matches(previous,0x78) && neighbor_matches(next,0x74);
}
```

`scene_destructor()` in `omega_dialogue_dispatch_probe.cpp` still forwards the original destructor even when diagnostic resolution fails. Its resolution logging is observational. The removal acknowledgement does not call extra native cleanup or synthesize an unlink.

## 5. Load the bookend state and use host teleport

The bookend constants in `omega_ending_rules.h` are:

- Bubble: `15`; authored state: `1`; global-state byte: `0x81`.
- Target slice/current region: `121`.
- Arrival spawn set: `0xAB06CC27`.
- Ending registry: `0x3A6CE17A`; registry tag: `0x80F47BCE`.
- Cinematic definition: `0x80F47BCB`; type: `6`; slot: `0`.
- Cinematic resource: `0x80C177DD`; selector: `0x2FECC6FD`; resource offset: `0x10578`.

Once `bookendState` is allowed, `activity_global_state_push.cpp` selects bubble 15 state `0x81` and slice 121. That loads the authored placement. `omega_ending::project_transit()` separately pins the exact activity incarnation and member, then runs the native host teleport transaction.

The transaction starts only from idle, issues a new nonzero teleport token, accepts arrival only for matching token/region/spawn with local state 3, and completes release when local state returns to 0:

Source: [Dawn/src/state/activity/omega_ending_transit_rules.h:79](<C:/Destiny 2 Development/Dawn/src/state/activity/omega_ending_transit_rules.h:79>) (lines 79-115).

```cpp
    [[nodiscard]] constexpr bool begin(Scope scope, Target target,
                                       Observation native) noexcept {
        if (!scope.valid() || !target.valid()) { return false; }
        if (phase_ != Phase::idle) { return scope == scope_ && target == target_; }
        // Membership retains hasTeleport once any native message22 reports it.
        // Before the first host command, a client may never have published that
        // optional field: its untouched default tuple is valid bootstrap input.
        // An unreported nondefault tuple is not evidence of an idle client.
        if (native.local.state != 0 || (!native.hasTeleport && native.local != Teleport{})) { return false; }
        scope_ = scope;
        target_ = target;
        auto next = static_cast<std::uint8_t>(native.local.token + 1U);
        if (next == 0) { next = 1; }
        host_ = {1, next, target.region, target.spawnSet};
        phase_ = Phase::requesting;
        return true;
    }

    /** True only on the first exact native local3/current-target arrival. */
    [[nodiscard]] constexpr bool observe(Scope scope, Observation native) noexcept {
        if (scope != scope_ || !scope.valid() || !native.hasTeleport
            || native.local.token != host_.token
            || native.local.sliceSetIndex != target_.region
            || native.local.sliceSetHash != target_.spawnSet) { return false; }
        if (phase_ == Phase::requesting && native.local.state == 3
            && native.hasRegion && native.currentRegion == target_.region) {
            host_.state = 3;
            phase_ = Phase::releasing;
            arrived_ = true;
            return true;
        }
        if (phase_ == Phase::releasing && native.local.state == 0) {
            host_.state = 0;
            phase_ = Phase::complete;
        }
        return false;
    }
```

The membership publisher carries this host authority. `project_transit()` forwards the exact local-state-3/current-region-121 receipt into `observe_arrival()`. Selecting the advertised target by itself does not satisfy arrival.

## 6. Offer the native cinematic

The bookend uses the generic cinematic authority schema `0x80804F08`. The host writes the controller revision and play flag to the ending type-6 slot; native authority application at `+0x10697B0` owns playback:

Source: [Dawn/src/middleware/bap/activity_message/activity_sensor_auth_bodies.cpp:658](<C:/Destiny 2 Development/Dawn/src/middleware/bap/activity_message/activity_sensor_auth_bodies.cpp:658>) (lines 658-665).

```cpp
[[nodiscard]] bool write_omega_ending(bits::Writer& writer,const Snapshot& snapshot) noexcept {
    const bool bookend=snapshot.omegaEndingState==ending::kState;
    return writer.write(0,64) && writer.write(0,64)
        && writer.write(bookend?snapshot.omegaEndingRevision:0U,32)
        && writer.write(bookend && snapshot.omegaEndingPlay?1U:0U,1) && writer.write(0,1)
        && write_dialogue_ref_absent(writer) && writer.write(2,6)
        && writer.write(0,5) && writer.write(0,3) && writer.write(0,32);
}
```

`omega_lair_cinematic.cpp` observes the actual ending component after native tick/apply. Its resource hook records the owner of `0x80C177DD`; the selector lookup must resolve to that recorded owner. It reads applied revision at component `+0x190` and active playback at `+0x260`:

Source: [Dawn/src/client/hooks/bootflow/omega_lair_cinematic.cpp:1701](<C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/omega_lair_cinematic.cpp:1701>) (lines 1701-1719).

```cpp
    if(identity(prefix.data(),ending::kDefinition,0x80804F07U,0x2E8)) {
        const auto command=ending::authority(navigation.run,GetTickCount64());
        if(!command.token.valid() || !command.bookendState) { return; }
        std::array<std::byte,0x270> bytes{};
        if(!copy(instance,bytes)) { return; }
        const auto lookup=g_lookup.load(std::memory_order_acquire);
        const auto owner=g_endingResourceOwner.load(std::memory_order_acquire);
        bool ready=false;
        if(lookup!=nullptr && owner!=UINT32_MAX) {
            const std::uint32_t selector=ending::kSelector;
            const auto* entry=lookup(&selector);
            std::array<std::byte,8> header{};
            ready=entry!=nullptr && copy(entry,header)
                && at<std::uint32_t>(header.data()+4)==owner;
        }
        ending::observe(command.token,at<std::uint32_t>(bytes.data()+0x190),
            at<std::uint8_t>(bytes.data()+0x260)!=0,ready);
        return;
    }
```

The controller requires arrival, a ready resource, and an inactive camera before offering play. It then requires the matching revision to observe activation and completion. A skip publishes stop authority with a new revision and still waits for a ready, inactive native receipt.

These are the actual receipt-handling cases, including camera retry and skip:

Source: [Dawn/src/state/activity/coo/omega_ending_controller.h:194](<C:/Destiny 2 Development/Dawn/src/state/activity/coo/omega_ending_controller.h:194>) (lines 194-228).

```cpp
    void apply(const Receipt& receipt) noexcept {
        switch(receipt.kind) {
        case Kind::retirement:
            if(phase_==native::Phase::retiring) { retired_=true;observed("lair.retired"); }break;
        case Kind::arrival:if(stateRequested_) { arrived_=true; }break;
        case Kind::skip:
            skipPending_=false;
            if(phase_==native::Phase::playing && play_) { play_=false;revision_=nextRevision_++; }break;
        case Kind::handoff:
            if(handoff_!=native::Handoff::claimed) { break; }
            handoff_=receipt.active?native::Handoff::queued:native::Handoff::failed;
            if(receipt.active) { observed("handoff.queued"); }
            else {
                Driver driver(*this);
                static_cast<void>(executor_.enqueue({executor_.token("handoff.queued"),Milestone::failed}));executor_.update(driver);
            }
            break;
        case Kind::camera:
            if(phase_==native::Phase::preparing && arrived_ && receipt.arrivedAtIntake && receipt.ready && !receipt.active) {
                observed("camera.eligible");
            } else if(phase_==native::Phase::offered && receipt.revision==revision_) {
                if(receipt.active && receipt.ready) { observed("camera.active"); }
                else if(!receipt.active && now_>=offeredAt_ && now_-offeredAt_>=1000U) {
                    if(attempts_>=3U) { fail(); }
                    else {
                        Driver driver(*this);executor_.cancel(driver);retry_=true;phase_=native::Phase::preparing;
                        if(!executor_.start(script::graph("ending_retry", kRetry),token_.run)) { fail();return; }drive();
                    }
                }
            } else if(phase_==native::Phase::playing && receipt.revision==revision_ && !receipt.active && receipt.ready) {
                observed("camera.complete");
            }
            break;
        }
    }
```

If an offered revision is still inactive after 1 second, the controller retries camera acquisition, up to three offers. The `ending_retry` graph does not repeat Lair retirement or teleport. Pending dialogue/retirement/preparation/offer phases have a 120-second failure watchdog; playback has a 300-second failure watchdog. Those timers report failure, not success.

The return-to-Lighthouse runtime receives registration defaults only during the arrived-and-offered window:

Source: [Dawn/src/middleware/bap/activity_message/sensor_auth_update.h:20](<C:/Destiny 2 Development/Dawn/src/middleware/bap/activity_message/sensor_auth_update.h:20>) (lines 20-23).

```cpp
[[nodiscard]] constexpr bool ending_runtime_seed_required(bool bookendState, bool arrived,
    bool play, bool started, bool failed) noexcept {
    return bookendState && arrived && play && !started && !failed;
}
```

`activity_sensor_auth_encoder.cpp` uses that predicate to replace the normal phase-two runtime publication with `write_ending_runtime_seed()` plus dialogue publication, and emits the ending streamed group. Once playback starts, the seed predicate becomes false.

## 7. After the movie: Mercury handoff

The graph retires the play command and requests handoff after native movie completion. `world_step.cpp` calls `omega_activity_handoff::poll()` from the game-frame owner.

The handoff constructs and validates a native selection for activity 29 (`mercury_freeroam`), sets the authored landing tuple, carries the current fireteam nonce when available, and claims the ending token once. The dispatch portion is:

Source: [Dawn/src/client/hooks/bootflow/omega_activity_handoff.inl:318](<C:/Destiny 2 Development/Dawn/src/client/hooks/bootflow/omega_activity_handoff.inl:318>) (lines 318-330).

```cpp
    if(!valid(selection.data()) || !ending::claim_handoff(token)) { return; }
    if(!state::activity::forced::suspend_omega_for_completed_run(token.run)) {
        ending::note_handoff_result(token,false);ending::update();report("override_changed",token);return;
    }
    if(!carriesNonce) { report("nonce_missing",token); } // Falls back to the orbit round trip.
    clear();
    select(0,selection.data());
    commit(1);
    ending::note_handoff_result(token,true);
    ending::update();
    exit_state()={token.run,carriesNonce?nonce:0,now,now,session,false};
    report("queued",token,nonce); // Native svc6 and in-world arrival remain separate receipts.
}
```

The rest of `omega_activity_handoff.inl` waits for the launch descriptor, selects the `no_ship` transition classification, and reports the native in-world exit to cleanup using step 28/reason 309. A temporary cinematic-suppression window covers the subsequent loading transition. This is the cleanup after the bookend; the Lair roster retirement above makes entry into the bookend possible. A queued launch is recorded separately from native service-6 launch and destination arrival.

## 8. Reading a stalled ending

- Still waiting on `final dialogue`: check the qualified encounter ending and final dialogue submission/playback window.
- Still waiting on `native Lair retirement`: inspect explicit zero-presence publication and `ev=omega_ending stage=native_retirement`. `before_guard` means the incoming removal or owner was not qualified. `after_guard` means the post-call state or token failed validation. The accepted receipt logs `reason=native_cleanup_complete ... accepted=1`.
- `bookendState` is set but arrival is missing: inspect the host teleport token, region 121, spawn `AB06CC27`, and matching native local-state-3 receipt.
- Arrived but play is not offered: check `stage=resource_registered`, selector ownership, and an inactive ready camera sample.
- Play offered but not started: compare the applied native revision with the offered revision and inspect the camera retry step.
- Movie stopped but completion is missing: check that native activation was previously accepted and that the inactive receipt has the current revision and a ready resource.
- Movie complete but destination unchanged: inspect `ev=omega_handoff stage=native_launch`, `native_exit`, and `transition_kind`.

## Source verification

All code blocks were extracted directly from the linked workspace files and checked against those source ranges. This was a documentation-only change; no build, unit-test executable, or live mission was run.

Existing relevant tests are [coo_ending_tests.cpp](<C:/Destiny 2 Development/Dawn/unit/coo_ending_tests.cpp>) for controller/wire equivalence and receipt ordering, and [coo_ending_runtime_tests.cpp](<C:/Destiny 2 Development/Dawn/unit/coo_ending_runtime_tests.cpp>) for the ending facade, transit, queued callbacks, skip, and handoff. These links describe available coverage, not new test results.

