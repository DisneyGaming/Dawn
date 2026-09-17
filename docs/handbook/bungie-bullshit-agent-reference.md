# Dawn activity and mission systems agent reference

---
document: dawn_activity_mission_agent_reference
version: 2026-08-22
audience: ai_agent
language_mode: strict_controlled_technical_english
evidence_date: 2026-08-22
project: Dawn
primary_branch: red-war-gameplay-host
observed_base_revision: 1ea4cb7b455bd5f075501ef33f705c8a07054eeb
source_sha256: 4BB36C14F9AEA1519FD4BBDF35C57B21AF5C016E14C897FED16A28402D248F39
source_path: C:\\Destiny 2 Development\\deliverables\\Dawn-complete-source-and-Release-20260822\\documentation\\DAWN-ACTIVITY-AND-MISSION-SYSTEMS-HANDBOOK-20260822.md
---

## 1. Purpose and status

Use this reference when an AI agent investigates or changes Dawn activity and mission systems.

This reference converts the human handbook into explicit rules, gates, procedures, and record formats.

This reference does not authorize work beyond the user request.

This reference is not a Bungie specification.

This reference combines source analysis, decoded data, logs, controlled tests, and reverse engineering.

The observed worktree contained local changes. The base revision does not identify every observed source state.

The deployed DLL can differ from the worktree. Verify both identities before each runtime conclusion.

This document uses controlled technical English. It is not a certified ASD-STE100 document.

The available language skill does not include the official ASD-STE100 approved-word dictionary.

### 1.1 Precedence

Apply these authorities in this order:

1. System safety and workspace rules
2. The current user request
3. Verified current source and runtime evidence
4. This agent reference
5. Historical evidence
6. Inference and hypotheses

Do not use this reference to expand the authorized task.

Do not let a case-specific rule override a verified general invariant.

### 1.2 Normative words

`MUST` identifies a required action or invariant.

`MUST NOT` identifies a prohibited action.

`SHOULD` identifies the preferred action when no stronger constraint applies.

`MAY` identifies an optional action.

`STOP` identifies a mandatory investigation boundary.

## 2. Agent operating contract

An agent MUST complete this contract before a material change.

1. Read the relevant source and current evidence.
2. State the authorized action.
3. Name the current system layer.
4. Name the last proved layer.
5. Name the first unproved layer.
6. Apply one evidence label.
7. State one testable hypothesis.
8. Change one independent input.
9. Define the positive result.
10. Define the negative result.
11. Define the stop condition.
12. Define the rollback.
13. Verify the actual postcondition.

An agent MUST preserve unrelated user changes.

An agent MUST inspect a dirty worktree before an overlapping edit.

An agent MUST use exact identities from the active run.

An agent MUST distinguish local recorder labels from engine identities.

An agent MUST NOT report a function return value as a semantic result.

An agent MUST NOT repeat a `DISPROVEN` route without new evidence.

An agent MUST remove obsolete probes after a bounded experiment.

An agent SHOULD use `rg` for file and log searches.

An agent SHOULD use `apply_patch` for source and documentation edits.

### 2.1 Authorization boundaries

A diagnosis request authorizes read-only inspection and evidence collection.

A diagnosis request does not authorize a fix.

A change request authorizes the smallest normal implementation for that change.

A build request authorizes compilation. It does not authorize deployment unless the request includes deployment.

A runtime test request authorizes the specified test. It does not authorize unrelated experiments.

A documentation request authorizes documentation changes only.

STOP when completion requires a materially different action.

Request user direction when the missing choice can change the result materially.

### 2.2 Required task record

Create this record before each bounded experiment:

```yaml
task:
  authorized_action: ""
  target: ""
  current_layer: ""
  last_proved_layer: ""
  first_unproved_layer: ""
  evidence_label: ""
  observation: ""
  hypothesis: ""
  single_change: ""
  positive_result: ""
  negative_result: ""
  stop_condition: ""
  rollback: ""
  verification: ""
```

Do not start the experiment when `single_change` contains multiple independent changes.

Do not start the experiment when `stop_condition` is empty.

## 3. Evidence contract

Every material conclusion MUST use one evidence label.

### 3.1 `CONFIRMED`

Direct source, decoded data, a log, or a controlled test proves the statement.

Record the exact evidence location.

### 3.2 `CURRENT SOURCE`

The current worktree implements the statement.

This label does not prove that the deployed DLL contains the implementation.

### 3.3 `HISTORICAL`

An earlier branch, build, or runtime capture validated the statement.

This label does not prove current behavior.

### 3.4 `INFERRED`

Available evidence supports the statement, but direct proof is absent.

Identify the evidence that supports the inference.

### 3.5 `HYPOTHESIS`

The statement is a testable proposal.

Do not write a hypothesis as a fact.

### 3.6 `DISPROVEN`

A controlled test rejected the predicted result.

Record the tested build, input, output, and stop reason.

### 3.7 `UNKNOWN`

Available evidence cannot identify the behavior.

Treat `UNKNOWN` as a valid research boundary.

### 3.8 Evidence preservation rules

Never increase confidence during a rewrite.

Never merge `CURRENT SOURCE` and `HISTORICAL` into `CONFIRMED`.

When source and history differ, current source defines current code behavior.

Historical evidence can still define protocol possibilities.

The deployed DLL defines runtime behavior only after identity verification.

A screenshot proves visible output only.

A screenshot does not prove which code executed.

A log line proves only the recorded event and its verified context.

Use the full correlation key before a cross-layer conclusion.

## 4. Core system invariants

`CONFIRMED`: Destiny 2 is an online world client.

The client contains maps, models, animations, effects, and native runtime systems.

The service supplies session state, authority, membership, mission state, and event inputs.

Dawn must reproduce sufficient service state for the native client.

Apply these invariants:

- A loaded package is not a mission.
- A loaded map is not mission authority.
- A session is not an actor.
- An actor is not a visual effect.
- A valid packet is not applied state.
- Applied state is not constructed runtime state.
- Constructed runtime state is not consumer output.
- Visible output is not gameplay progression.
- A successful call is not a successful postcondition.

Proof at one layer does not prove the next layer.

The agent MUST identify the first unproved boundary.

### 4.1 Preferred execution path

The host simulation path is preferred.

Dawn publishes service state on this path.

The native client performs the mission on this path.

The client executor path uses hooks or direct native calls.

Use the client executor for diagnosis or a required build-specific bridge.

The client executor can bypass native ownership and lifecycle.

Do not treat executor success as host simulation success.

## 5. End-to-end layer model

Use these canonical layers:

1. Platform bootstrap
2. BAP and account services
3. Destination selection
4. Activity-session allocation
5. World, bubble, slice, and spawn selection
6. Public or private route selection
7. Gameplay peer and group transport
8. Activity membership and entity slots
9. Bubble authority
10. Roster phase 1
11. Native runtime object construction
12. Auth and sense phase 2
13. Mission director and activity script
14. Sensors and triggers
15. Scene authority and selector
16. Actors, models, animations, and events
17. VFX and renderer consumption
18. Completion and progression

### 5.1 Layer gate rule

For each layer, record these values:

```yaml
layer_gate:
  layer: 0
  required_input: ""
  observed_input: ""
  expected_output: ""
  observed_output: ""
  owner: ""
  evidence: ""
  status: pass|fail|unknown
```

Advance only when the layer output is proved.

When a layer fails, investigate its producer and consumer.

Do not compensate at a later layer before the failed boundary is known.

### 5.2 Success levels

Use these result levels:

1. `transport`: The receiver accepted the frame or packet.
2. `decode`: The receiver decoded the intended fields.
3. `state`: The stable owner stored the intended state.
4. `construction`: The native runtime constructed the intended object.
5. `presentation`: The intended visual or audio output appeared.
6. `gameplay`: The intended interaction or encounter behavior occurred.
7. `lifecycle`: Native transitions and retirement occurred correctly.
8. `mission`: Progression and successor state occurred correctly.

Name the highest proved success level in every result.

Do not use the word `fixed` without the required success level.

### 5.3 Mission reconstruction levels

Use these completion levels:

1. Destination complete
2. Session complete
3. Authority complete
4. Presentation complete
5. Gameplay complete
6. Lifecycle complete
7. Progression complete
8. Repeatability complete

Never report mission completion without the level number.

Level 8 requires repeated clean runs from the same defined baseline.

## 6. Identity rules

Treat every identity domain as separate.

### 6.1 Canonical identity domains

- Account handle
- SOID
- Activity-session identifier
- Activity-host identifier
- Group-session identifier
- Machine identifier
- Member key
- Online-session identifier
- Advertisement identifier
- Object reference
- Scene handle and generation
- Effect handle and generation

Never substitute one identity because two values look similar.

Never compare only a low handle index.

Always include generation for reusable native handles.

An object reference uses a biased type and index.

Apply the type bias `+1` exactly once.

Apply the index bias `+32768` exactly once.

### 6.2 Run-local labels

Recorder names such as `Scene 1` are run-local labels.

Factory numbers are also run-local labels unless engine evidence proves otherwise.

Record the full native identity beside every local label.

Use this mapping format:

```yaml
local_identity_map:
  local_label: ""
  native_handle: ""
  generation: ""
  definition: ""
  parent: ""
  first_seen_time: ""
  last_seen_time: ""
```

## 7. Project architecture and ownership

All source paths are relative to `C:\Destiny 2 Development\Dawn-src`.

### 7.1 Client layer

`Dawn/src/client` owns hooks, content extraction, and native integration.

### 7.2 Middleware layer

`Dawn/src/middleware` owns codecs, wire schemas, and message definitions.

### 7.3 Server layer

`Dawn/src/server` owns BAP routes and gameplay service behavior.

### 7.4 State layer

`Dawn/src/state` owns stable process state and transactions.

### 7.5 Platform layer

`Dawn/src/steam` owns platform emulation.

### 7.6 Core layer

`Dawn/src/core` owns settings, logging, and user-interface support.

### 7.7 Ownership rule

Place persistent state in the state layer.

Place encoding and decoding in middleware.

Place service sequencing in server code.

Place native process interaction in client code.

Do not store borrowed packet spans in persistent state.

Do not mutate global state from a native teardown callback.

## 8. BAP bootstrap and service routing

### 8.1 Frame types

`CURRENT SOURCE`: BAP supports these frame types:

- `0`: plaintext type 0
- `1`: encrypted
- `2`: plaintext type 2

### 8.2 Request and response rule

A request with a defined response identifier MUST receive a response.

Silence can block the client's pending-request ring.

Do not treat a neutral response as optional silence.

### 8.3 Current request services

`CURRENT SOURCE`: The route table accepts these request identifiers:

```text
6, 8, 10, 110, 12, 14, 16, 18, 21, 23, 25, 29, 30,
32, 34, 36, 38, 40, 42, 44, 48, 121, 171, 250, 302,
304, 306
```

### 8.4 Current response services

`CURRENT SOURCE`: The route table uses these response identifiers:

```text
7, 11, 112, 13, 15, 17, 19, 22, 24, 26, 31, 33, 35,
37, 39, 41, 43, 45, 49, 122, 251, 303, 305, 307
```

### 8.5 Server notifications

`CURRENT SOURCE`: Notification `9` carries an activity message.

`CURRENT SOURCE`: Notification `123` carries a queue update.

### 8.6 Staged service outcome

Use this transaction order:

1. Parse the request.
2. Validate all inputs.
3. Prepare the response.
4. Prepare the state mutation.
5. Encode and copy the response.
6. Revalidate mutable revisions.
7. Commit the state mutation.
8. Publish later notifications.

Do not commit state before response encoding succeeds.

Do not publish a notification before its state commit.

### 8.7 Sensitive data

Do not retain borrowed sensitive spans.

Do not log raw sensitive descriptors.

Log hashes, sizes, labels, and validated structural fields.

## 9. Destination selection

### 9.1 Service 6 request

`CURRENT SOURCE`: Service `6` expects exactly `7,719` bytes.

The request contains two destination selection copies.

Choose the primary copy unless it is unknown and the secondary copy is known.

Preserve these destination values:

- Package name
- Travel reason
- Previous activity
- Current activity
- Activity element
- Arrival hash
- Spawn hash
- Exact raw descriptor

Do not reconstruct the raw descriptor from known scalar values.

The unknown bits can contain required runtime data.

### 9.2 Destination validation

`CURRENT SOURCE`: Package names have a `40` character limit.

`CURRENT SOURCE`: Travel reason is from `-1` through `14`.

`CURRENT SOURCE`: Activity is `-1`, `0`, or `1` through `4094`.

`CURRENT SOURCE`: Activity element is `-1` or `0` through `510`.

`CURRENT SOURCE`: The raw descriptor uses at most `232` bytes and `1,814` bits.

`CURRENT SOURCE`: The common descriptor length is `372` bits.

`CURRENT SOURCE`: An absent spawn hash uses `0x811C9DC5`.

### 9.3 Arrival override

Apply the arrival override after request parsing and validation.

Do not change unrelated descriptor fields during an override.

### 9.4 Forced destination

Apply a forced destination after normal selection.

A forced destination is process-local state.

A world destination requires package, bubble, and slice values.

A cinematic destination can be bubbleless.

Validate bubble values from `0` through `63`.

Validate slice values from `0` through `1022`.

### 9.5 Service 7 response

`CURRENT SOURCE`: The Service `7` body is `137` bytes.

The body contains these fields:

1. Discriminator value `2`
2. Big-endian `u64` activity-session identifier
3. `128` zero bytes

The activity-session identifier MUST be nonzero.

### 9.6 Wire and runtime records

The BAP descriptor and the native `0x1B0` runtime record are different records.

`HISTORICAL`: The runtime record had a route byte at offset `+0x12`.

`HISTORICAL`: Zero selected a local path in one observed build.

`HISTORICAL`: A nonzero value selected an authored path in one observed build.

The offset and meanings are build-specific.

Do not copy a BAP bit offset into the native record.

Identify the producer of the exact runtime record that the consumer reads.

### 9.7 Recovered wire descriptor

`HISTORICAL`: The recovered descriptor included these ordered values:

- Four-bit travel reason
- Twelve-bit source activity
- Twelve-bit destination activity
- Optional nine-bit activity element
- Skull data
- Peer data
- Arrival hash
- Spawn hash
- Forty biased package-name bytes
- Trailing Boolean value
- Two trailing structures

`HISTORICAL`: Observed descriptor lengths were `372`, `620`, and `716` bits.

One `716` bit descriptor contained one `96` bit reference.

Descriptor length alone does not identify its contents.

`HISTORICAL`: One sensitive scalar matched the primary SOID.

`HISTORICAL`: Another sensitive scalar matched a per-load nonce.

The nonce changes between loads.

Service 6 acceptance does not prove runtime ingestion on the next load.

## 10. Activity sessions

### 10.1 Capacity and identifiers

`CURRENT SOURCE`: The process stores at most `16` activity sessions.

When full, the table evicts its oldest record.

Activity-session identifiers start at `1`.

Identifier `0` means absent.

Session revisions start at `1`.

Revisions MUST NOT wrap.

### 10.2 Session record

The activity-session record owns these domains:

- Destination
- Client-held entity slots
- Server-reserved entity slots
- Activity membership
- Bubble authority
- Session identifiers
- Revisions
- Join state
- Bound member key
- Occupied and joined flags

Client-held slots and server-reserved slots MUST be disjoint.

A returned client slot becomes free.

It does not become server-reserved.

### 10.3 Pending allocation

Prepare allocation before a state commit.

Record the source revision during preparation.

Revalidate the revision before the commit.

Reject a stale prepared allocation.

### 10.4 Group-host sessions

The group-host session table is separate from the activity-session table.

`CURRENT SOURCE`: The group-host table stores at most `8` sessions.

Do not use one table identifier as the other table identifier.

A regional host can inherit destination state from a source activity session.

Validate the source session before host allocation commits.

## 11. World, bubble, slice, and spawn state

### 11.1 Bubble and slice relation

`CURRENT SOURCE`: A world supports `64` bubbles.

`CURRENT SOURCE`: Each bubble supports at most `8` slice states.

Calculate the bubble as `sliceSetIndex >> 3`.

Do not equate the arrival bubble with current membership region.

### 11.2 Spawn selection

Spawn selection requires package availability.

The spawn hash uses FNV-1.

`CURRENT SOURCE`: The spawn catalog stores at most `65,536` points.

`CURRENT SOURCE`: The catalog stores at most `128` stems.

`CURRENT SOURCE`: The catalog stores at most `4,096` hashes.

`CURRENT SOURCE`: The bubble mask is `32` bytes.

`CURRENT SOURCE`: A spawn record stores at most `12` package references.

`CONFIRMED`: One observed catalog had `77` stems and `2,090` hashes.

### 11.3 World phases

Use these world phases:

1. `idle`
2. `transitioning`
3. `arrived`

Seed authored mission state only after the world reaches `arrived`.

Clear authored mission state when the world returns to `idle`.

### 11.4 Spawn hold

Hold spawn until the world reaches `arrived`.

Do not use native spawn permission as the only readiness signal.

`CURRENT SOURCE`: The default hold is `30,000` milliseconds.

`CURRENT SOURCE`: The maximum hold is `600,000` milliseconds.

An early spawn can produce a black screen.

## 12. Activity-host endpoint and route selection

### 12.1 Service 16 request

`CURRENT SOURCE`: Service `16` expects an exact eight-byte request.

The request contains one big-endian activity-host identifier.

Reject an invalid request size.

### 12.2 Service 17 response

`CURRENT SOURCE`: Service `17` returns exactly `16` bytes.

The response contains these fields:

1. Echoed big-endian activity-host identifier
2. Big-endian IPv4 address
3. Neutral `u16`
4. Big-endian `u16` port

The address and port MUST be nonzero.

The echoed identifier MUST match the request.

### 12.3 Private route

A forced destination can load a world through the private route.

Private world loading does not prove public mission behavior.

Do not use solo world loading as public route evidence.

### 12.4 Public route

The public route requires these domains:

- Region activity-host identifier
- Advertisement
- Citizen membership
- Gameplay group state
- Activity-host publication

The ambassador slot differs from the local slot.

`CURRENT SOURCE`: Local slot `0` selects ambassador slot `1`.

`CURRENT SOURCE`: Any other local slot selects ambassador slot `0`.

The advertisement activity-host identifier MUST match the group parameter.

A mismatch produces `public_activity_host_mismatch`.

### 12.5 Advertisement readiness

Use these readiness states:

- `ready`
- `pending`
- `absent`

Allocate the host session outside the staged push transaction.

Do not perform an irreversible host allocation during response preparation.

## 13. Matchmaking

### 13.1 Request kinds

`CURRENT SOURCE`: Matchmaking uses these request kinds:

```text
0  none
1  search
2  advertisement update
3  advertisement delete
4  configuration
5  rejoin update
6  rejoin delete
7  locate
8  statistics
```

### 13.2 Join descriptor

The join descriptor is exactly `128` opaque bytes.

Preserve every descriptor byte.

Do not reconstruct the descriptor from decoded fields.

### 13.3 State limits

`CURRENT SOURCE`: Matchmaking state supports `4` contexts.

`CURRENT SOURCE`: Each context supports `16` variants.

### 13.4 Current search behavior

`CURRENT SOURCE`: A session search returns a present empty field `3` result.

Do not describe this result as an omitted result.

Present empty data and omitted data can select different client branches.

### 13.5 Configuration

`CURRENT SOURCE`: The current configuration uses these values:

```text
service_count: 1
search_seconds: 60
desperation_seconds: 60
provider_count: 1
max_players: 9
posse_size: 3
matchmade_players: 6
service_identifier: 1
```

A present empty configuration is not a neutral configuration.

`HISTORICAL`: Zero timing thresholds caused an immediate advertisement change.

`HISTORICAL`: That change advanced generation and cancelled the active search.

Use valid nonzero policy values when the configuration is present.

Omit an optional policy when no valid meaning is known.

### 13.6 Historical search result

`HISTORICAL`: A descriptor-bearing result used this nesting:

1. Root field `3`
2. Repeated field `1`
3. Descriptor message field `1`
4. Descriptor bytes field `1`

`HISTORICAL`: Incorrect nesting caused policy `31` fatal decoding.

Do not restore this result without current route evidence.

### 13.7 Locate response

`CURRENT SOURCE`: A locate response uses at most `148` bytes.

Validate the exact encoded length before publication.

### 13.8 Matchmaking proof boundary

A decoded matchmaking response proves only decode success.

A search entry proves only matchmaking state.

An advertisement proves only advertisement state.

Public mission success requires later group and activity layers.

## 14. NAT introduction and QoS

### 14.1 NAT introduction

`HISTORICAL`: NAT request identifier `13` received reply identifier `12`.

Do not infer the current route from this historical pair alone.

### 14.2 QoS request

`HISTORICAL`: QoS request identifier `0x28` used `18` bytes.

The observed request fields were:

```text
byte 0       type
bytes 1-8    combined timestamp and payload-size value
bytes 5-8    requested payload size
bytes 9-12   probe key
bytes 13-16  session identifier
byte 17      trailing request byte
```

### 14.3 QoS response

`HISTORICAL`: QoS response identifier `0x29` had a recovered header.

The observed response fields were:

```text
byte 0       type
bytes 1-4    echoed probe key
bytes 5-12   echoed request bytes 1-8
byte 13      acceptance value 1
bytes 14-17  little-endian payload length
bytes 18-21  zero
bytes 22-25  zero
byte 26      zero
byte 27+     structured payload
```

`HISTORICAL`: A correct header cleared identifier, refusal, and empty-response errors.

The complete structured response remains unresolved.

`DISPROVEN`: A zero payload is a valid QoS response.

Do not claim a complete current QoS implementation.

STOP schema tuning when the client does not reach the QoS consumer.

## 15. Gameplay transport

### 15.1 Layer order

Use this gameplay network order:

1. Endpoint
2. Association
3. DTLS
4. Peer
5. Group
6. Parameters and membership
7. Activity host

Do not diagnose a later layer before the preceding layer passes.

### 15.2 Current local topology

`CURRENT SOURCE`: The default address is `127.0.0.1`.

`CURRENT SOURCE`: The default even port is `30976`.

Ports `3074` and `3075` are excluded.

Capture the exact `NetAddr` that the client presents.

Do not synthesize a replacement address when exact bytes are available.

### 15.3 Peer message identifiers

`CURRENT SOURCE`: Peer connection uses these identifiers:

```text
5  connect request
6  connect response
7  connect refusal
8  establish
9  close
10 group join request
14 group join refusal
```

### 15.4 Application-ready boundary

Peer establishment alone does not make the connection application-ready.

Application readiness requires establishment and one normal connected packet.

Before readiness, reliable packets can receive acknowledgements without application dispatch.

Do not publish group state before application readiness.

### 15.5 Packet discriminator

The first-bit grammar separates out-of-band and established packets.

Decode this discriminator before the established packet schema.

### 15.6 Association lifetime

`HISTORICAL`: The client attempted reconnect approximately `50` milliseconds after a close.

Do not release the association on an ordinary close.

Release the association on timeout or a terminal condition.

### 15.7 Reliable publication

Key the reliable queue by group-session identifier.

Do not share one reliable sequence across unrelated groups.

Preserve sequencing, acknowledgements, assembly state, and view signature.

An acknowledgement does not prove application dispatch.

### 15.8 View signature

A view-signature mismatch can block entity output.

Verify the signature before editing entity bodies.

## 16. Gameplay group session

### 16.1 Known session messages

`CURRENT SOURCE`: The known group messages are:

```text
11 peerConnect
12 joinComplete
13 joinAbort
15 leave
16 acknowledgment
17 disband
18 boot
19 handoff
21 transition
22 hostReestablish
25 peerReestablish
26 peerEstablish
29 timeSync
30 membership
```

### 16.2 Known decoded sizes

`CURRENT SOURCE`: The known decoded body sizes are:

```text
peerConnect:       24
joinComplete:      24
joinAbort:         24
leave:              8
acknowledgment:     8
disband:           24
boot:              24
handoff:          100
transition:        16
hostReestablish:  136
peerReestablish:    8
peerEstablish:      8
timeSync:          48
membership:     31104
```

Reject a size mismatch before decoding fields.

### 16.3 Membership capacity

`CURRENT SOURCE`: Group membership supports `32` members.

`CURRENT SOURCE`: Group membership supports `32` players.

Publish complete snapshots.

Increase membership revisions strictly.

### 16.4 Member state ladder

Observed member states include these values:

```text
3  reserved
5  connected
7  joined
8  waiting
9  ready
10 established
```

The local member must reach state `10` for full establishment.

Do not skip an observed required state without evidence.

### 16.5 Self-member resolution

Find the local member with exact `NetAddr` and join identifier data.

Do not resolve the local member by array position alone.

Connection-present state controls address resolution.

### 16.6 Membership hash

The trailing membership hash MUST match the encoded snapshot.

Recalculate the hash after each body mutation.

### 16.7 Host reestablish

`HISTORICAL`: The host-reestablish body contained identities at offsets `0x66` and `0x76`.

`HISTORICAL`: Their observed lengths were `16` and `18` bytes.

`HISTORICAL`: Empty identities could return success while the manager remained inactive.

Verify manager activation as the postcondition.

Do not use the call result as activation proof.

### 16.8 Host handoff

Handoff arms a pending member index.

The handoff address MUST match the target address byte for byte.

### 16.9 Host transition

Validate transition count from `0` through `100`.

Reject a count outside this range.

## 17. Group parameters and publication order

### 17.1 Parameter registry

`CURRENT SOURCE`: The registry uses parameter indices `0` through `24`:

```text
0  worldControllerGoalData
1  activeJoinControls
2  atomicCascadeJoinData
3  activityHost
4  jipGate
5  activitySelection
6  activitySelectionResponses
7  currentActivity
8  previousActivity
9  userJoinControls
10 desiredJoinControls
11 language
12 requestedRemoteJoinData
13 remoteJoinData
14 remoteJoinResult
15 hostSelected
16 matchmakingMessaging
17 matchmakingAbortRequested
18 sessionDisbandReason
19 matchmakingProgress
20 initialSliceSetStatus
21 publicSessionReservations
22 matchmakingData
23 matchmakingPeerData
24 networkQuality
```

`CURRENT SOURCE`: Parameter update uses message `38`.

`CURRENT SOURCE`: Parameter request uses message `39`.

### 17.2 Minimum publication

Publish membership and at least one group parameter.

An empty parameter set is not a complete join.

The `activityHost` parameter MUST contain a nonzero identifier.

A zero `activityHost` value can latch an unusable state.

### 17.3 Required order

Use this publication order:

1. Reach application-ready state.
2. Publish membership.
3. Publish required parameters.
4. Promote the local member.
5. Publish activity-host messages.

Preserve the observed migration sequence during a host transition.

Verify the manager state after the final message.

## 18. Activity-message protocol

### 18.1 Envelope

The activity-message envelope contains these ordered fields:

1. Big-endian `u64` account handle
2. Byte value `1`
3. Big-endian `u32` message type
4. Big-endian `u32` body length
5. Big-endian `u32` heard mask
6. Exact body bytes

`CURRENT SOURCE`: Maximum body length is `0x7D800` bytes.

Reject a body that exceeds this limit.

### 18.2 Server-to-client types

`CURRENT SOURCE`: The server publishes these known types:

```text
0  entity slots
1  global state
4  join
5  auth and sense
12 membership
54 bubble authority table
```

### 18.3 Client-to-server types

`CURRENT SOURCE`: The client uses these known types:

```text
3  join
6  sense
8  request activity host
11 start
13 reservation operation
14 reservation operation
15 leave
16 keepalive
18 refresh
19 incident
20 slot operation
21 slot operation
22 authoritative update
23 identity
26 abandon
27 purge
29 reset acknowledgment
31 query
32 query
33 abdicate
34 debug
37 connectivity
38 membership acknowledgment
39 heartbeat
43 bug report
46 lag
47 quality
48 migration
49 high-water mark
50 inspiration
52 patch epoch
```

Unknown message types MUST retain their exact body for analysis.

### 18.4 Delivery plans

`CURRENT SOURCE`: A handled request can schedule these notification plans:

- No notification
- Join notifications
- Entity-slot notification
- Membership notification
- Refresh notifications
- Authoritative notifications

`CURRENT SOURCE`: A request can mutate one of these state domains:

- None
- Entity slots
- Membership
- Patch epoch

Keep mutation and notification plans in one staged result.

### 18.5 Refresh and movement order

Use this refresh order:

1. Global state
2. Membership
3. Roster

A region move requires immediate membership and roster publication.

Do not wait for an unrelated periodic refresh.

An authoritative update can also require membership or roster refresh.

Publish membership when host state changes.

The new bubble needs roster and grant state before usable authority exists.

### 18.6 Transition token

A transition-token change starts a load boundary.

It also increases roster publication frequency.

Record the old and new token.

Do not compare activity state across tokens without explicit mapping.

Do not change the token during an ordinary stable refresh.

### 18.7 Patch epoch

Patch epoch is connection-owned state.

Reset patch epoch when its connection epoch changes.

Do not store patch epoch as unrestricted process-global state.

Echo the exact patch epoch during auth and sense publication.

An incorrect epoch can suppress phase 2 without a clear decode error.

## 19. Global state, membership, slots, and authority

### 19.1 Global state

`CURRENT SOURCE`: Global state contains `1,161` meaningful bits.

Its byte storage uses `146` bytes.

The state includes these values:

- Sixty-four bubble bytes
- Initial slice
- Spawn hash
- Destination descriptor
- Activity state
- Transition state

A valid global-state body does not prove downstream application.

### 19.2 Activity membership

`CURRENT SOURCE`: Local activity membership contains `29,968` bits.

Its byte storage uses `3,746` bytes.

A remote citizen adds `1,081` bits and a `1,024` bit descriptor.

Keep membership epoch `0` stable until a real epoch change exists.

An epoch change clears the peer table.

### 19.3 Citizen identity

The citizen member key MUST match its descriptor machine identity.

The citizen `NetAddr` MUST match the advertisement address exactly.

The citizen online-session identifier uses the ambassador activity-host identifier.

The citizen region uses the target region.

The ambassador identity MUST differ from the local identity.

The local member key MUST match the joined activity session.

The local player identity MUST match the gameplay group row.

### 19.4 Regional identities

`CURRENT SOURCE`: Regional identities derive from a base value and this stride:

```text
0x9E3779B97F4A7C15 * (region + 1)
```

Apply the operation exactly as the current source defines it.

Do not infer identity equivalence from a shared base.

### 19.5 Entity slots

`CURRENT SOURCE`: The entity-slot space contains `8,192` slots.

The slot mask contains `1,024` bytes.

`CURRENT SOURCE`: The default server reserve is `256` slots.

`CURRENT SOURCE`: The minimum server reserve is `8` slots.

`CURRENT SOURCE`: The client requires at least `4,096` slots.

A disabled gameplay topology reserves zero server slots.

Reserve high-numbered slots for the server.

Grant low-numbered slots to the client.

The two sets MUST remain disjoint.

The client requests slots with message Type `20`.

The client returns slots with message Type `21`.

Commit the slot mutation before publishing Type `0`.

### 19.6 Slot proof boundary

A granted slot proves allocation only.

A slot does not prove roster registration.

A roster entry does not prove native object construction.

### 19.7 Bubble authority

`CURRENT SOURCE`: The authority table contains `65` entries.

Entries `0` through `63` represent usable bubbles.

Entry `64` is a fallback entry.

Never grant fallback entry `64`.

Grant tokens start at `1`.

Token `0` means cleared authority.

Tokens reset with their bounded activity-session record.

A new region requires a new authority grant.

The destination can identify slice `1022`.

The current grant representation supports values only through `511`.

Never truncate a larger slice value into the grant representation.

Report the representation mismatch as a boundary.

## 20. Build data and content extraction

### 20.1 Content proof ladder

Use this content proof ladder:

1. Package tag found
2. Package loaded
3. Scenario selected
4. Registry discovered
5. Registry mounted
6. Roster group published
7. Native runtime object constructed
8. Object body applied
9. Component active
10. Consumer output observed

Never skip a proof level in a conclusion.

A package tag at level `1` does not prove a runtime object at level `7`.

### 20.2 Scenario catalog

`CURRENT SOURCE`: The scenario catalog stores at most `512` scenarios.

`CONFIRMED`: One observed build contained `468` scenarios.

`CURRENT SOURCE`: A scenario name uses at most `40` characters.

`CURRENT SOURCE`: A scenario contains at most `64` bubbles.

`CURRENT SOURCE`: Enabled state uses `0x80`.

`CURRENT SOURCE`: Disabled state uses `0x7F`.

A scenario can also contain these records:

- Package tag
- Group masks
- Spawn stem
- Initial slice hash and index
- Map index
- Declared package list

### 20.3 Roster catalog

`CURRENT SOURCE`: The roster catalog stores at most `128` groups.

`CURRENT SOURCE`: It stores at most `1,280` slots.

`CONFIRMED`: One observed build had `68` groups and `1,218` slots.

`CONFIRMED`: The observed maximum slot type was `72`.

`CURRENT SOURCE`: A scenario can use four top-level registries.

`CURRENT SOURCE`: Each bubble can use four bubble-local registries.

`CURRENT SOURCE`: A spawn stem uses at most `32` characters.

`CURRENT SOURCE`: A roster group stores at most `8` package references.

### 20.4 Roster group record

Record these fields for every roster group:

```yaml
roster_group:
  registry_key: ""
  object_tag: ""
  slot_count: 0
  slot_types: []
  flags: ""
  explicit_indices: []
  registry_scope: top_level|bubble_local
  source_bubble: ""
  source_slice: ""
```

An ordinal is not an authored index.

Host-only data can be absent from a client package.

Do not manufacture missing host-only entries.

### 20.5 Staged scenario build

Use this build order:

1. Collect candidate scenarios.
2. Resolve package and registry references.
3. Compact stable results.
4. Traverse registries.
5. Publish the complete catalog.

An empty first collection pass does not prove absence.

Retry only at the defined build boundary.

Do not publish a partial catalog as complete.

### 20.6 Registry traversal

Traverse all bubbles.

Traverse all slices in each bubble.

Traverse all three registry arrays in each slice.

Record source scope for every discovered key.

### 20.7 Top-level and bubble-local groups

Compute safe top-level keys as an intersection across every slice.

Remove a top-level key when any required slice lacks it.

`HISTORICAL`: A key lost during slice teardown could crash the client.

Keep bubble-local groups local to their source bubble.

Do not promote a bubble-local group to global scope without proof.

`CURRENT SOURCE`: The selected top-level set requires Type `13` participation.

`CURRENT SOURCE`: It also requires Type `17` lifetime coverage.

### 20.8 Authored exception

`CURRENT SOURCE`: `rosterForceAuthored` enables an exact Omega-specific exception.

The default value is `false`.

Do not treat this exception as a universal roster rule.

Changing this setting rebuilds the content cache.

Document each keyed exception with mission, purpose, and removal condition.

## 21. Auth and sense publication

### 21.1 Phase 1

Phase 1 publishes these values:

- Roster groups
- Entity slots
- Ownership flags
- Bubble authority grants

Phase 1 does not publish complete object bodies.

Phase 1 naming does not prove runtime construction.

### 21.2 Phase 2

Phase 2 publishes object bodies.

Phase 2 MUST use the exact active connection epoch.

Publish top-level bodies before bubble-local bodies.

`CURRENT SOURCE`: One update contains at most `8` groups.

### 21.3 Slot flags

Use flag `1` for sense ownership.

Use flag `2` for auth ownership.

Do not infer body semantics from ownership flags alone.

A slot can carry one flag or both flags.

Encode only bodies that the slot flags permit.

### 21.4 Object references

Use the object-reference bias rules from Section 6.

Do not apply a bias during both preparation and encoding.

An authored object index MUST NOT exceed `32,767`.

### 21.5 Exact remainder

Encode the exact remaining bit count.

Do not pad a logical field count to a guessed byte boundary.

Calculate the complete body length before publication.

Verify the final writer position against that length.

### 21.6 Reset target

The first reset MUST target the active live mirror.

Do not reset a stale copy and report success.

### 21.7 State sequence

State sequence identifies an object generation.

`HISTORICAL`: The first three warmup updates could advance the sequence.

After warmup, the sequence remained stable in observed runs.

A sequence change tears down associated runtime objects.

Do not increment the sequence for an ordinary body update.

Change it only for a group-set change or a required rebuild.

### 21.8 Runtime existence ladder

Use this object existence ladder:

1. Package tag exists.
2. Scenario includes the group.
3. Phase 1 includes the group.
4. Native factory constructs the object.
5. Phase 2 body reaches the object.
6. Component becomes active.
7. Consumer produces output.

STOP body tuning when step `4` is absent.

Investigate construction ownership before another body experiment.

## 22. Serialization rules

### 22.1 Bit and byte count

Calculate byte count as `(bit_count + 7) / 8`.

Preserve the meaningful bit count separately.

### 22.2 Endianness

Endianness is field-specific.

Do not apply one endianness rule to an entire protocol.

Record endianness beside every recovered scalar.

### 22.3 Protobuf presence

An absent field differs from a present zero field.

An absent message differs from a present empty message.

Preserve original presence when semantics are unknown.

### 22.4 Opaque data

Preserve exact opaque bytes.

Do not normalize unknown padding or tail data.

Preserve these known opaque domains:

- Activity-selection descriptor
- Join descriptor
- Identity blocks
- Unknown manager payloads
- Native effect definitions

### 22.5 Biased values

Apply each field bias exactly once.

Record whether the stored value is biased or unbiased.

### 22.6 Optional branches

An optional branch changes later bit offsets.

Decode the presence condition before later fields.

Do not align later fields from a different branch.

### 22.7 Neutral body

A neutral body MUST remain schema-valid.

A zero-filled body is not necessarily neutral.

Use a recovered neutral body when available.

Build one explicit neutral encoder for each recovered type.

## 23. Core roster object types

### 23.1 Type 13 participation

`CONFIRMED`: Type `13` uses `192` required bits.

It can include an optional `32` bit value.

It binds SOID, player identity, and region state.

Its biased identities cannot use zero-filled placeholders.

### 23.2 Type 17 activity lifetime

`CONFIRMED`: Type `17` uses `520` bits.

Observed safe states include `3`, `6`, and `10`.

`CURRENT SOURCE`: The current neutral state is `3`.

The native dispatcher uses an unbounded jump table.

Do not publish an untested state value.

The body includes spawn overrides and a waiting-state switch.

Type `17` can control lifecycle boundaries.

Do not hold its final state without a completion plan.

### 23.3 Type 18 activity script

`INFERRED`: Type `18` is an activity-script object.

Its recovered schema identifier is `0x80809919`.

Type `18` is not the complete mission.

### 23.4 Type 35 mission director

`INFERRED`: Type `35` is a mission-director object.

Its recovered schema identifier is `0x808099BF`.

Type `35` is not the complete mission.

### 23.5 Shared script state

The shared schema identifier is `0x808099C4`.

The recovered state includes these ordered fields:

1. One Boolean value
2. Five `u64` values
3. One `u32` value

One observed validity-window value was `0x0000134F00C00000`.

This value represents approximately `365` days in the observed time unit.

`DISPROVEN`: An all-zero shared tail provides a valid neutral window.

Use an explicit valid window in a neutral shared state.

### 23.6 Supporting types

`CURRENT SOURCE`: Type `8` configuration uses `35` bits.

`CURRENT SOURCE`: Type `16` package state uses `7` bits.

`CURRENT SOURCE`: Type `41` queues uses `12` bits.

`CURRENT SOURCE`: Type `67` spawn keys uses `1,057` bits.

Type `68` can carry a directive.

Type `53` can carry dialogue state.

Type `11` can carry music state.

Directive, dialogue, and music objects can use neutral bodies.

## 24. Recovered mission object schemas

### 24.1 Recovered definitions

The following entries are recovered schema evidence:

```text
type  schema      known bit count
1     80807EC9    24 neutral, 355 active
4     8080992F    253
23    80804F48    147
26    8080954B    171
30    80809532    41
31    80809524    66
34    8080956A    1
43    8080626B    37
70    808094F1    23
```

Treat each bit count as schema-specific evidence.

Do not transfer a body between different schema identifiers.

### 24.2 Type 4 active body

The recovered Type `4` active body uses this field order:

1. One `32` bit value
2. One `32` bit value
3. Active Boolean value
4. Resolve Boolean value
5. One `32` bit value
6. Nested key with `32`, `7`, and `16` bit parts
7. Three `32` bit vector values
8. One Boolean value
9. Inherited two-bit state
10. Optional `32` bit value

Do not reorder fields by semantic appearance.

Encode the recovered wire order.

### 24.3 Type 43 selector

The recovered Type `43` body contains these ordered values:

1. One `u32` selector
2. One Boolean value
3. One four-bit value

Publish an inactive selector before the active selector.

### 24.4 Type 1 spawner

Type `1` can create a scene actor.

The active example contains nested generation counts and request counts.

It also contains a squad-device reference.

A network spawner can duplicate an actor that the native scene already creates.

Do not add Type `1` when native actor construction already exists.

### 24.5 Unresolved active bodies

The active meanings of Types `26` and `31` remain unresolved.

Do not invent active semantics for these bodies.

Use `UNKNOWN` until direct evidence exists.

## 25. Mission seed and readiness

### 25.1 Seed timing

Seed authored mission state only after world arrival.

Do not seed while the world phase is `idle` or `transitioning`.

### 25.2 Seed setting

`CURRENT SOURCE`: `seedAuthoredSensors` controls authored sensor seeding.

The default value is `false`.

Do not enable it as a general mission setting.

### 25.3 Readiness edge

Use a verified host readiness edge.

After readiness, change one mission field per test.

Do not use elapsed time as a substitute for readiness.

### 25.4 Connection publication

Store publication state per connection.

Rollback publication state when a send operation fails.

Do not report publication from prepared but unsent data.

### 25.5 Deferred mutation

Defer state mutation from native allocator and teardown callbacks.

Perform mutation on the owning state path.

This rule prevents reentrant ownership corruption.

## 26. Mission director and activity script

### 26.1 Authority role

Mission director and activity script objects coordinate mission authority.

They do not replace all sensors, scenes, spawners, and devices.

### 26.2 Neutral initialization

Initialize mission objects with schema-valid neutral bodies.

Establish a valid native consumption window before an active body.

Do not send an active body before the object exists.

### 26.3 Scalar progression

Use one scalar or latch per bounded test.

Record the previous value and the new value.

Do not force a final value to imitate a missing lifecycle.

Keep destination, roster, patch epoch, and selector constant.

Record publication time and the first native response.

`HISTORICAL`: One Omega experiment used this case-specific model:

```text
0 inactive
1 ready
2 triggered
3 after portal
```

This model is not a universal mission state machine.

### 26.4 Lifecycle edges

Prefer native sensor and lifecycle edges.

Do not replace a native trigger with a timer unless the user requests a temporary bridge.

Publish stable successor state before completion.

Do not retire the current phase before its successor can own the mission.

### 26.5 Mission handoff

A mission handoff requires these proved conditions:

1. Current phase has a valid owner.
2. Successor state is published.
3. Successor object is active.
4. Current phase reports completion.
5. Current presentation can retire safely.

If any condition is absent, report the first absent condition.

## 27. Sensors and triggers

### 27.1 Trigger evidence

Use real Type `6` sense input when available.

Do not infer a trigger from proximity alone.

Record these fields for each trigger event:

```yaml
trigger_event:
  activity_session: ""
  group: ""
  slot_type: ""
  slot_index: ""
  object_handle: ""
  generation: ""
  bubble: ""
  connection_epoch: ""
  bit_count: 0
  time: ""
  result: ""
```

### 27.2 Trigger identity

Use full group, type, index, handle, and generation.

Do not identify a trigger by one low index.

### 27.3 Omega example

`CONFIRMED`: Omega used trigger `D00142CF` in one case-specific run.

`CONFIRMED`: The trigger was Type `30`, index `20`.

Do not apply this trigger to another mission without evidence.

### 27.4 Debounce

Debounce duplicate native trigger reports.

Key debounce state by full object identity and connection epoch.

Clear debounce state when the object generation changes.

Reset a one-shot latch only at a defined mission lifecycle boundary.

### 27.5 Trigger proof boundary

A sensed trigger proves sensor input.

It does not prove mission state mutation.

Verify the mission owner's postcondition after the trigger.

## 28. Scene system

### 28.1 Scene layers

Treat a scene as these separate layers:

1. Scene authority
2. Selector state
3. Timeline
4. Cast scheduler
5. Entity factory
6. Behavior
7. Presentation
8. Body, cloth, and head models
9. Authored event
10. Transform bank and provider
11. Visual effects
12. Renderer
13. Phase report
14. Retirement

Do not collapse these layers into one `scene` result.

### 28.2 Scene authority

Scene authority selects and owns the active scene state.

The selector value is not an actor identity.

The selector value is not necessarily a local scene number.

### 28.3 Selector initialization

Publish an inactive selector before an active selector.

Do not assume the client initializes missing selector state safely.

### 28.4 Phase report

A scene phase report can request a mission handoff.

Scene retirement can complete a mission phase.

Record both events separately.

`CONFIRMED`: One Omega Type `43` state changed from `108` to `140` bits.

`CONFIRMED`: The `140` bit state armed handoff in that run.

It did not prove final completion.

Native actor retirement supplied the later observed completion edge.

### 28.5 Audio boundary

Dialogue audio is separate from actor construction.

Audio without actors proves only the audio path.

Do not report scene construction from dialogue alone.

### 28.6 Scene isolation

Enable one cast or scene role at a time.

Use an A/B matrix for scene isolation.

Begin with no enabled cast.

Record audio, actors, effects, and mission state.

Then enable one entry and repeat the same route.

Record every enabled combination and visible result.

Do not infer causation from a run with multiple changed casts.

### 28.7 Local scene labels

Labels such as `Scene 1`, `Scene 2`, and `Scene 3` are recorder labels.

Map each label to full native identities for each run.

Do not treat the number as engine semantics.

## 29. Scene phase roles

### 29.1 Entry role

An entry scene introduces a phase or begins an action.

It can end before the mission phase ends.

### 29.2 Transition role

A transition scene changes one presentation or mission state into another.

It can contain temporary actors and effects.

### 29.3 Persistent role

A persistent scene presents the stable state after a transition.

It can remain active while mission logic continues.

### 29.4 Omega evidence

`CONFIRMED`: The Omega label `Scene 1` produced the correct Ikora action.

`CONFIRMED`: This isolated run did not produce the detached purple defect.

`CONFIRMED`: The Omega label `Scene 2` produced purple effects and an unwanted body.

`CONFIRMED`: The Omega label `Scene 3` produced the stable final presentation.

`CONFIRMED`: Disabling all mission actors preserved dialogue audio.

These labels describe the recorded Omega run only.

### 29.5 Retained transition

Retaining a transition scene can suppress its retirement signal.

An infinite presentation can block normal mission progression.

Add a replacement completion edge only at a proved lifecycle boundary.

Do not use an arbitrary timer as the replacement edge.

## 30. Actor and entity construction

### 30.1 Complete actor identity

Record these values for each actor:

```yaml
actor_identity:
  activity_session: ""
  scene_handle: ""
  scene_generation: ""
  cast_handle: ""
  cast_generation: ""
  entity_handle: ""
  entity_generation: ""
  definition: ""
  presentation: ""
  root_transform: ""
  model_resources: []
  first_seen_time: ""
  last_seen_time: ""
```

### 30.2 Root and socket

An actor root transform is not an attachment socket.

A socket can be selected after root composition.

Do not redirect an effect to the actor root without socket evidence.

### 30.3 Native scene actor

A native scene can construct its own actor.

Verify native construction before publishing a network spawner.

### 30.4 Network spawner

A network spawner is a separate actor construction route.

Do not add it to repair an already constructed native actor.

Duplicate construction can produce overlapping actors or incorrect retirement.

### 30.5 Actor retirement

Actor retirement can signal scene or mission completion.

Do not suppress actor retirement without tracing its lifecycle consumer.

## 31. Model and presentation construction

### 31.1 Separate model resources

Body, head, and cloth can use separate model resources.

Suppressing one resource does not suppress the other resources.

### 31.2 Exact suppression

Filter an unwanted model by complete construction identity.

Preserve scene timeline, events, actor logic, and effects.

Do not disable an entire scene when only one presentation resource is wrong.

### 31.3 Omega evidence

`CONFIRMED`: One exact Omega filter removed the unwanted body.

`CONFIRMED`: The head remained after body removal.

`CONFIRMED`: A second exact filter removed the head.

`CONFIRMED`: Purple effects remained after both model filters.

This evidence separates presentation resources from effect ownership.

### 31.4 Suppression boundary

Model suppression changes presentation.

The actor and scene logic can remain active.

Verify lifecycle behavior after a presentation filter.

Also verify collision and invisible logic.

## 32. Visual effects

### 32.1 Required ownership questions

For every effect defect, identify these owners:

1. Who creates the effect?
2. Which authored event requests it?
3. Which effect definition is active?
4. Which transform bank supplies candidates?
5. Which provider selects the transform?
6. Which composite stores endpoints?
7. Which renderer consumes final data?
8. Which destructor ends the effect?

Do not change a transform before these identities are separated.

### 32.2 Complete effect identity

Record these values:

```yaml
effect_identity:
  activity_session: ""
  effect_handle: ""
  generation: ""
  definition: ""
  scene_handle: ""
  scene_generation: ""
  authored_event: ""
  selector: ""
  transform_bank: ""
  provider: ""
  composite: ""
  renderer_input: ""
  create_time: ""
  first_render_time: ""
  destroy_time: ""
```

### 32.3 Transform bank

A transform-bank index is not an actor number.

A transform-bank index is not a scene number.

Map bank entries to their producers before substitution.

### 32.4 Provider selection

A provider can select a socket before the observed callback.

Changing the callback actor can leave the selected socket unchanged.

Trace provider input and provider output.

### 32.5 Direct-bank route

Some effects can read a transform bank directly.

A direct-bank effect can ignore a callback actor substitution.

Prove the actual read route before another actor redirection.

### 32.6 Composite and renderer

The composite can copy or transform provider output.

Later code can rewrite copied endpoints.

A successful memory write and readback prove memory only.

They do not prove renderer consumption.

Identify the final renderer input before a binding fix.

### 32.7 Separate consumers

Aura and beam effects can use different consumers.

Do not assume one binding controls all purple presentation.

Filter and trace each full effect identity separately.

### 32.8 Scene effect and mission visual

A scene effect can differ from a persistent mission visual.

Do not treat a transition VFX as the final portal state.

### 32.9 Bounded transform experiment

Use this exact procedure:

1. Filter one complete effect identity.
2. Capture its original transform.
3. Wait until the known composer completes.
4. Add one large visible offset.
5. Confirm the write.
6. Find any later rewrite.
7. Observe one visual result.
8. Restore the original transform.

STOP when the final renderer input does not change.

Mark that route `DISPROVEN`.

### 32.10 Omega result

`CONFIRMED`: The detached Omega effects persisted after exact model suppression.

`DISPROVEN`: Direct actor-context substitution fixed the final effect binding.

`DISPROVEN`: A `+10` Scene-bank position write moved the rendered effect.

`DISPROVEN`: A filtered live-effect vector write moved the rendered effect.

`UNKNOWN`: The exact final Omega effect binding remains unidentified.

The remaining owner is later than, or separate from, the tested composite path.

Do not resume these routes without new producer-to-consumer evidence.

## 33. World devices, portals, and barriers

### 33.1 Separate states

Treat these as separate runtime objects or states:

- Closed barrier
- Transition animation
- Open portal

Do not assume one scene owns all three states.

### 33.2 Registry ownership

A discovered definition does not prove a mounted registry.

A mounted registry does not prove object construction.

Trace the device through the content proof ladder.

### 33.3 Foreign-registry evidence

`CONFIRMED`: One Omega foreign-registry test named the correct phase 1 key.

`CONFIRMED`: The test did not construct the runtime object.

This result disproves phase 1 naming as sufficient construction proof.

### 33.4 Device stop rule

STOP body edits when the native factory object is absent.

Choose one remaining construction route:

1. Mount the native registry.
2. Invoke the native construction path.
3. Use a compatible existing slot with proved semantics.

Test one route at a time.

### 33.5 Collision and presentation

Visual state and collision state can have separate owners.

Verify both states before reporting a portal or barrier complete.

## 34. Completion and progression

### 34.1 Completion sources

A completion edge can originate from these systems:

- Activity script
- Sensor
- Scene phase report
- Actor retirement
- Spawner
- World device
- Objective
- Manager

Identify the exact owner before a replacement edge.

### 34.2 Successor first

Publish stable successor state before current-phase completion.

Verify successor activation before current presentation retirement.

### 34.3 Infinite scene rule

An infinite scene changes presentation lifetime.

It does not automatically preserve logical mission progress.

Track these states separately:

```yaml
lifecycle_state:
  retained_presentation: false
  handoff_reported: false
  mission_phase_complete: false
  successor_active: false
```

### 34.4 Progression proof

Mission progression requires the owning mission state to advance.

Presentation change alone is not progression.

Verify the successor mission owner and durable state.

## 35. AI, encounters, and objectives

Actor animation is not AI behavior.

A spawner is not encounter control.

A HUD directive is not objective logic.

An enemy model is not pathing or combat authority.

Current general support for these systems remains incomplete:

- Full mission graph
- AI behavior
- Navigation and pathing
- Encounter control
- Objective logic
- Rewards
- Quest completion

Do not claim multiplayer encounter parity from scene reconstruction.

## 36. State and transaction design

### 36.1 Process-owned state

Process-owned state includes these domains:

- Activity sessions
- Current destination
- Entity slots
- Activity membership
- Bubble grants
- Matchmaking state
- Group-host sessions
- Scenario catalog
- Roster catalog
- Spawn catalog
- Durable mission stage

### 36.2 Connection-owned state

Connection-owned state includes these domains:

- Connection epoch
- Publication progress
- Warmup count
- Transition token
- Roster generation
- Trigger debounce
- Pending notifications
- Patch epoch

Do not store connection state in an unrestricted process singleton.

### 36.3 Client-native state

Native handles and pointers belong to the client layer.

Do not persist native pointers in server state.

Store stable identities and resolve native objects at the client boundary.

### 36.4 Prepare and commit

Use immutable prepared values.

Record the source revision during preparation.

Revalidate the revision before commit.

Commit all related values atomically.

Publish notifications after commit.

### 36.5 Monotonic revisions

Revisions MUST increase strictly.

Reject revision wraparound.

Reject a stale mutation.

### 36.6 Deferred callbacks

Do not perform owning-state mutation inside native allocation or teardown callbacks.

Queue a deferred mutation with stable identity data.

### 36.7 Capacity behavior

Define behavior for every fixed capacity.

Use one of these outcomes:

- Reject the new item.
- Evict a defined eligible item.
- Defer the operation.

Do not overwrite an arbitrary live entry.

## 37. Observability contract

### 37.1 Correlation key

Correlate each event with all available fields:

```yaml
correlation_key:
  process_id: ""
  executable_hash: ""
  dll_hash: ""
  activity_session: ""
  activity_host: ""
  group_session: ""
  connection_epoch: ""
  transition_token: ""
  roster_generation: ""
  scene_handle: ""
  scene_generation: ""
```

Do not join events across processes without explicit evidence.

Do not join events across transition tokens without explicit evidence.

### 37.2 Stable event form

Use stable event names and field names.

Log these common values:

```yaml
event:
  name: ""
  owner: ""
  previous: ""
  current: ""
  input: ""
  identity: ""
  revision: ""
  result: ""
  time: ""
```

### 37.3 Postcondition

Log the postcondition after every mutating call.

Example postconditions include these values:

- Stored revision
- Manager active state
- Runtime object count
- Body application count
- Component active state
- Renderer input change
- Mission successor state

Do not log only `returned=true`.

### 37.4 Absence records

Absence is useful evidence only inside a defined time window.

Log one bounded absence summary.

Include expected owner, expected event, window start, and window end.

Do not emit one absence line per frame.

### 37.5 Rate control

Rate-limit high-frequency observations.

Do not discard first occurrence, state change, or final summary.

### 37.6 Evidence archive

Archive these artifacts for a consequential run:

- Source revision
- Working-tree diff summary
- Untracked file list
- Executable hash
- DLL hashes
- Runtime settings
- Focused logs
- Screenshots or video
- Expected result
- Actual result
- Evidence label
- Rollback state

## 38. Runtime recorder design

### 38.1 Recorder question

Write one sentence that states the recorder question.

Example:

> Which final renderer input receives effect definition X during scene generation Y?

Do not build a recorder that answers several unrelated questions.

### 38.2 Fixed record fields

Prefer fixed-size records in native hooks.

Use these fields when applicable:

```text
event
handle
generation
definition
parent
scene
actor
input
output
transform
thread
time
```

Avoid dynamic allocation in high-frequency hooks.

### 38.3 Entry and exit

Record function entry and exit.

Record input and output values separately.

This separation identifies which function changed data.

### 38.4 Provenance chain

Trace provenance in this direction:

```text
creator -> authored event -> definition -> provider -> composite -> final consumer
```

Do not stop at the first writable structure.

### 38.5 Capture window

Use the shortest window that includes creation and first consumption.

Stop capture after the required terminal event.

### 38.6 Executable identity

Verify the executable and DLL identities before interpreting addresses.

Reject a capture from an unknown build.

## 39. Static reverse engineering

### 39.1 Analysis image

The observed unpacked image path is:

```text
C:\Destiny 2 Development\destiny2_unpacked.bin
```

The observed image base was `0x7FF618070000`.

For that image only, file offset equaled RVA.

Do not generalize this equality to another image.

### 39.2 RVA validation

Every RVA is build-specific.

Before using an RVA, verify these values:

1. Executable hash
2. Image base
3. Expected bytes
4. Calling convention
5. Relevant cross-references

STOP when expected bytes differ.

Do not call or patch the address.

### 39.3 Schema recovery

Recover both reader and writer when possible.

Record these properties:

- Field order
- Field width
- Signedness
- Endianness
- Bias
- Presence condition
- Nested structure
- Default value
- Bounds
- Side effect

Do not infer a complete schema from one writer alone.

### 39.4 Strings and jump tables

Use native strings for navigation only.

A nearby string does not prove function ownership.

For an unbounded jump table, use only confirmed selector values.

Do not enumerate guessed selectors in the live client.

### 39.5 Protected code

When code is protected, capture stable inputs and outputs.

Do not guess internal semantics when boundary evidence is sufficient.

## 40. Build and deployment

### 40.1 Pre-build record

Before a build, record these values:

1. Current branch
2. Base revision
3. Modified file list
4. Staged file list
5. Untracked file list
6. Relevant settings
7. Intended experiment

Git diff does not include untracked files.

Record untracked files separately.

### 40.2 Build command

Use this approved build helper:

```powershell
powershell -ExecutionPolicy Bypass -File "C:\Destiny 2 Development\dawn-dev.ps1" -Config Release -BuildOnly
```

`BuildOnly` compiles the project.

It does not prove deployment into the active game process.

### 40.3 Deployment safety

Close the game before replacing a loaded DLL.

Verify the destination path before replacement.

Record the deployed DLL hash.

### 40.4 Post-build record

After a build, record these values:

- Build result
- Output DLL hash
- Deployed DLL hash
- Startup marker
- Runtime settings
- Active experiment identifier

### 40.5 Logs

Use these log paths:

```text
C:\Destiny 2 Development\bin\x64\Dawn\logs\dawn.log
C:\Destiny 2 Development\bin\x64\Dawn\logs\dawn.log.old
```

Do not use a screenshot as proof that a hook executed.

Require a build marker or recorder event.

## 41. Bounded experiment method

### 41.1 Required statement

Every experiment MUST define these fields:

```yaml
experiment:
  observation: ""
  hypothesis: ""
  independent_input: ""
  positive_result: ""
  negative_result: ""
  stop_condition: ""
  rollback: ""
```

### 41.2 Baseline

Run the unchanged baseline twice when runtime variance is possible.

Record whether both runs produce the same relevant result.

Do not test a change against an unstable baseline.

### 41.3 One variable

Change one independent input per build or run.

Instrumentation can observe several values without changing them.

### 41.4 Visible probe

Use one large visible offset when small motion can be ambiguous.

Use the visible probe once.

Restore the original value after the result.

### 41.5 Binary result

Prefer a result with two possible interpretations.

Define both interpretations before the run.

### 41.6 Negative result

A negative result removes the tested branch.

Record it as `DISPROVEN` when the test was controlled.

Do not weaken a clear negative result to preserve a preferred theory.

### 41.7 Stop condition

STOP when the final consumer disproves the route.

STOP when the required producer is absent.

STOP when the active build identity is unknown.

STOP when more work requires a new user authorization.

### 41.8 Cleanup

Remove obsolete offsets, probes, filters, and recorder noise.

Keep only production behavior and required diagnostics.

## 42. Symptom-to-layer decision rules

### 42.1 No secure service activity

Inspect platform bootstrap, BAP frame decode, and route dispatch.

Do not inspect mission bodies.

### 42.2 Service 6 succeeds but no world loads

Verify Service `7`, nonzero session identifier, stored destination, and native runtime record production.

### 42.3 World loads at the wrong location

Verify package, bubble, slice, arrival hash, spawn hash, and exact descriptor selection.

### 42.4 World loads but the screen remains black

Verify world phase, spawn hold, spawn-set resolution, and arrival readiness.

### 42.5 Public search does not start

Verify route mode, region activity host, advertisement, ambassador identity, and matchmaking configuration presence.

### 42.6 Peer connects but group does not join

Verify application readiness, reliable queue ownership, membership body, and local member resolution.

### 42.7 Group joins but no activity host appears

Verify parameter publication order and nonzero `activityHost` data.

Verify manager activation as the postcondition.

### 42.8 Activity host joins but no roster appears

Verify global state, membership, slots, bubble grants, and phase 1 publication order.

### 42.9 Phase 1 names a group but no object appears

Inspect registry mounting and the native factory path.

STOP body tuning.

### 42.10 Runtime object exists but body does not apply

Verify connection epoch, state sequence, object reference bias, bit count, and body schema.

### 42.11 Mission object applies but scene does not start

Verify mission scalar, selector initialization, scene authority, and native lifecycle edge.

### 42.12 Dialogue plays but no actor appears

Inspect cast scheduler, entity factory, and presentation construction.

Do not edit the audio path.

### 42.13 Actor appears in a default pose

Inspect behavior state, animation binding, cast ownership, and lifecycle timing.

Do not classify the default pose as a VFX issue.

### 42.14 Effect appears at the wrong location

Trace creator, event, provider, bank, composite, and final renderer input.

Do not redirect the first writable transform.

### 42.15 Visual device is absent

Verify registry discovery, registry mount, factory construction, body application, presentation, and collision.

### 42.16 Scene looks correct but mission does not progress

Inspect phase reports, retirement, mission owner state, and successor activation.

Do not extend presentation lifetime as the first fix.

## 43. Prohibited reasoning patterns

Each item below is a known invalid inference.

1. Package present means runtime object present.
2. Phase 1 name means native construction occurred.
3. Accepted packet means stable state changed.
4. Function success means semantic activation occurred.
5. Dialogue audio means actor scene constructed.
6. Character model means VFX owner.
7. Actor root means effect socket.
8. Memory write means renderer consumed the value.
9. Low handle index means stable object identity.
10. Recorder factory number means actor identity.
11. Final state write means lifecycle completed.
12. Zero data means neutral data.
13. Present empty means omitted.
14. Timer means native trigger.
15. Infinite scene means normal mission completion.
16. Several simultaneous changes identify one cause.
17. More tests remain useful after the consumer disproves the route.

An agent MUST reject these inferences unless new direct evidence changes the boundary.

## 44. Mission reconstruction state machine

Advance through these phases in order.

Return to an earlier phase when a later result invalidates its gate.

### 44.1 Phase A: baseline

Required actions:

1. Record executable and DLL identities.
2. Record source and settings.
3. Run the unchanged path twice.
4. Record the last proved layer.
5. Record the first unproved layer.

Exit condition:

> The relevant baseline is repeatable and correlated.

### 44.2 Phase B: transport and sessions

Required actions:

1. Verify BAP request and response pairs.
2. Verify destination parsing.
3. Verify nonzero activity-session allocation.
4. Verify activity-host endpoint data.
5. Verify gameplay application readiness.

Exit condition:

> Transport, session, and endpoint state are proved.

### 44.3 Phase C: world state

Required actions:

1. Verify package selection.
2. Verify bubble and slice.
3. Verify spawn-set resolution.
4. Verify world arrival.
5. Verify transition token.

Exit condition:

> The client reaches the intended world and valid spawn state.

### 44.4 Phase D: authority and roster

Required actions:

1. Publish global state.
2. Publish membership.
3. Allocate disjoint slots.
4. Grant bubble authority.
5. Publish phase 1 groups.
6. Verify native object construction.
7. Publish phase 2 bodies.

Exit condition:

> Required native runtime objects exist and consume valid bodies.

### 44.5 Phase E: mission control

Required actions:

1. Initialize mission objects with neutral bodies.
2. Verify host readiness.
3. Apply one active mission field.
4. Capture native sensor input.
5. Verify mission owner mutation.

Exit condition:

> A native trigger advances the intended mission owner.

### 44.6 Phase F: presentation isolation

Required actions:

1. Isolate each scene role.
2. Map local labels to native identities.
3. Verify actor construction.
4. Verify model resources.
5. Trace effect provenance.
6. Verify final consumer output.

Exit condition:

> Each required presentation element has a proved owner and correct output.

### 44.7 Phase G: completion

Required actions:

1. Identify the native completion source.
2. Publish the successor state.
3. Verify successor activation.
4. Permit current phase retirement.
5. Verify durable mission progression.

Exit condition:

> The mission advances through native lifecycle and successor ownership.

### 44.8 Phase H: devices and encounters

Required actions:

1. Mount required registries.
2. Construct world devices.
3. Apply presentation and collision state.
4. Construct encounter controllers.
5. Verify AI, objectives, rewards, and completion independently.

Exit condition:

> Gameplay systems operate through proved native owners.

### 44.9 Phase I: diagnostic cleanup

Required actions:

1. Remove obsolete probes.
2. Remove disproven experimental paths.
3. Retain bounded production diagnostics.
4. Run the non-regression checklist.
5. Repeat the clean mission baseline.

Exit condition:

> The result is repeatable without experimental residue.

## 45. General case evidence

### 45.1 Homecoming lessons

`HISTORICAL`: A working farm run emitted `ready for instantiation`.

Therefore, that phrase alone is not a failure.

`HISTORICAL`: The exact public transition input started citizen and search paths.

`HISTORICAL`: Ambassador identity relationships selected different client branches.

`HISTORICAL`: A structurally valid matchmaking configuration materialized a client lane.

`HISTORICAL`: Observed manager modes were `1`, `2`, `4`, and `5`.

`HISTORICAL`: Observed manager payload sizes were `0x17F8`, `0x1880`, `0x90`, and `0xA0`.

`HISTORICAL`: Route `1` reused the same observed nonce.

Route entry does not prove full manager simulation.

`HISTORICAL`: Kind `22` returned success while active state remained zero.

The body used empty identity fields in that run.

`HISTORICAL`: The recorder label `identity 2` identified the local identity in one run.

Treat this label as run-local.

`HISTORICAL`: Zero Service `7` metadata did not block route `1` in one controlled test.

Do not generalize that result to other manager routes.

### 45.2 Omega lessons

`CONFIRMED`: A neutral mission seed followed by a real Type `6` trigger advanced the tested path.

`CONFIRMED`: Scene isolation separated entry, transition, and persistent presentation roles.

`CONFIRMED`: Exact model filters removed unwanted models while effects remained.

`DISPROVEN`: Several investigated transform routes controlled the detached final VFX location.

`CONFIRMED`: Retaining the transition scene changed retirement behavior.

`CONFIRMED`: Foreign phase 1 registry publication did not construct the wall object.

Use these lessons as boundaries, not universal mission values.

## 46. Current implementation boundaries

### 46.1 Current strengths

`CURRENT SOURCE`: Dawn contains substantial support for these systems:

- BAP framing and service routing
- Destination parsing and validation
- Activity-session allocation
- Forced destination selection
- Activity-host endpoint response
- Matchmaking request and response structures
- Gameplay advertisement
- Gameplay group-host state
- Activity-message parsing and publication
- Global activity state
- Activity membership
- Entity-slot transactions
- Bubble-authority transactions
- Scenario catalog extraction
- Roster extraction
- Spawn-set catalog extraction
- Auth and sense publication
- Spawn hold
- Build-specific client hooks

### 46.2 Current cautions

The worktree can differ from the base revision.

The deployed DLL can differ from the worktree.

Case-specific settings can change general behavior.

Build-specific addresses can become invalid after an executable update.

Some historical routes are absent from current source.

### 46.3 Known gaps

These areas remain incomplete or unresolved:

- Complete QoS response semantics
- General public matchmaking parity
- Complete mission graph reconstruction
- Complete schema coverage for every roster slot type
- Active schemas for several recovered mission objects
- Universal registry mounting
- General native entity construction lifecycle
- General world-device construction
- Exact Omega VFX final binding
- General AI and pathing
- General encounter control
- Objective and reward logic
- Quest completion
- Stable Homecoming manager identity construction
- Build-independent native hook locations
- Full multiplayer parity

### 46.4 Architectural ceiling

Loading authored content cannot replace missing service authority.

Hooks can demonstrate native behavior without recreating host semantics.

A mission is complete only when its required layers reach the declared completion level.

At a construction ceiling, select one explicit architecture:

1. Recover native registry mounting.
2. Recover the native entity factory.
3. Reuse a compatible mounted object with full schema proof.
4. Use an authorized temporary client-executor bridge.

## 47. Runtime settings

Record every relevant setting before a test.

Current important defaults are:

```yaml
settings:
  fadeRelease: true
  forceJoinRequestReady: true
  regionPrivate: false
  pinReplicatedRecord: true
  holdSpawn: true
  spawnHoldMs: 30000
  rosterForceAuthored: false
  seedAuthoredSensors: false
```

`fadeRelease` controls fade-release behavior.

It releases the world-transition fade channel at the in-world step.

It bridges a spawn path that misses normal release.

`forceJoinRequestReady` changes join-readiness handling.

It forces the observed status `5` to `6` readiness check.

Two terms are local flags with no known host mutation.

Treat this setting as a client bridge.

`regionPrivate` changes public or private region behavior.

`false` permits the public activity-host route.

`true` reports a public region as private and loads it solo.

`pinReplicatedRecord` keeps the selected participation record stable.

It selects the replicated record instead of an inactive local copy.

The inactive copy contains an unreachable spawn-gate byte.

`holdSpawn` enables the arrival hold.

`spawnHoldMs` defines its duration.

`rosterForceAuthored` enables an Omega-specific roster exception.

`seedAuthoredSensors` enables authored sensor seeding.

Core diagnostic slot types include `13`, `16`, `17`, `18`, `35`, and `41`.

This set is foundational diagnostic state.

It is not a complete mission roster.

## 48. Source map

All paths in this section are relative to `C:\Destiny 2 Development\Dawn-src`.

### 48.1 BAP frames and transactions

```text
Dawn/src/middleware/bap/frame.h
Dawn/src/server/bap/encrypted/routing/bap_service_routing.cpp
Dawn/src/server/bap/encrypted/transactions/service_outcome_commit.cpp
```

### 48.2 Destination and activity allocation

```text
Dawn/src/server/bap/encrypted/activity_host_manager/activity_host_manager_route.cpp
Dawn/src/middleware/bap/activity_host_manager/request/activity_manager_request.cpp
Dawn/src/middleware/bap/activity_host_manager/request/selection/activity_manager_selection_parser.cpp
Dawn/src/middleware/bap/activity_host_manager/request/selection/activity_manager_selection_descriptor.cpp
Dawn/src/middleware/bap/activity_host_manager/response/activity_manager_response.cpp
Dawn/src/state/activity/definition.h
Dawn/src/state/activity/destination/definition.h
Dawn/src/state/activity/destination/activity_destination_validation.cpp
Dawn/src/state/activity/destination/activity_destination_spawn_binding.cpp
Dawn/src/state/activity/forced/activity_forced_destination.cpp
```

### 48.3 Matchmaking

```text
Dawn/src/middleware/bap/matchmaking/definition.h
Dawn/src/middleware/bap/matchmaking/request/matchmaking_request_parser.cpp
Dawn/src/middleware/bap/matchmaking/response/matchmaking_response_encoder.cpp
Dawn/src/server/bap/encrypted/matchmaking/matchmaking_route.cpp
Dawn/src/state/matchmaking/definition.h
Dawn/src/state/matchmaking/transactions/matchmaking_prepare.cpp
Dawn/src/state/matchmaking/transactions/matchmaking_commit.cpp
```

### 48.4 Advertisement and group host

```text
Dawn/src/server/gameplay/gameplay_advertisement.cpp
Dawn/src/server/gameplay/group/group_host.cpp
Dawn/src/server/gameplay/group/group_host_sessions.cpp
Dawn/src/middleware/gameplay/group/parameter_registry.h
```

### 48.5 Activity messages

```text
Dawn/src/server/bap/encrypted/activity_message/activity_message_route.cpp
Dawn/src/middleware/bap/activity_message/activity_message_request_parser.cpp
Dawn/src/middleware/bap/activity_message/activity_message_notification_encoder.cpp
Dawn/src/server/bap/encrypted/push/activity/activity_message_push.cpp
```

### 48.6 Global state and membership

```text
Dawn/src/middleware/bap/activity_message/activity_global_state_encoder.cpp
Dawn/src/middleware/bap/activity_message/activity_replicate_membership_encoder.cpp
Dawn/src/middleware/bap/activity_message/activity_membership_member_writer.cpp
Dawn/src/middleware/bap/activity_message/activity_membership_region_writer.cpp
Dawn/src/server/bap/encrypted/push/activity/activity_membership_push.cpp
Dawn/src/state/activity/membership/definition.h
Dawn/src/state/activity/membership/transactions/activity_membership_prepare.cpp
Dawn/src/state/activity/membership/transactions/activity_membership_commit.cpp
```

### 48.7 Entity slots and bubble authority

```text
Dawn/src/middleware/bap/activity_message/activity_entity_slots_encoder.cpp
Dawn/src/middleware/bap/activity_message/activity_entity_slots_decoder.cpp
Dawn/src/state/activity/entity_slots/definition.h
Dawn/src/state/activity/entity_slots/transactions/activity_entity_slot_prepare.cpp
Dawn/src/state/activity/entity_slots/transactions/activity_entity_slot_commit.cpp
Dawn/src/state/activity/bubble_authority/definition.h
Dawn/src/state/activity/bubble_authority/transactions/activity_bubble_authority_grant.cpp
```

### 48.8 Scenario and roster extraction

```text
Dawn/src/client/content/scenarios/scenario_build.cpp
Dawn/src/client/content/scenarios/scenario_collect.cpp
Dawn/src/client/content/scenarios/scenario_roster_build.cpp
Dawn/src/client/content/scenarios/scenario_roster_groups.cpp
Dawn/src/client/content/scenarios/scenario_roster_publish.cpp
Dawn/src/client/content/scenarios/scenario_slot_classification.cpp
Dawn/src/state/build_data/scenarios/definition.h
Dawn/src/state/build_data/scenarios/scenario_catalog.cpp
Dawn/src/middleware/content/packages/tables/scenario_reader.cpp
Dawn/src/middleware/content/packages/tables/scenario_walk.cpp
```

### 48.9 Spawn sets

```text
Dawn/src/client/content/spawn_sets/spawn_set_build.cpp
Dawn/src/client/content/spawn_sets/spawn_set_catalog_builder.cpp
Dawn/src/state/build_data/spawn_sets/definition.h
Dawn/src/state/build_data/spawn_sets/spawn_set_catalog.cpp
```

### 48.10 Auth and sense

```text
Dawn/src/middleware/bap/activity_message/sensor_auth_update.h
Dawn/src/middleware/bap/activity_message/activity_sensor_auth_encoder.cpp
Dawn/src/middleware/bap/activity_message/activity_sensor_auth_blocks.cpp
Dawn/src/middleware/bap/activity_message/activity_sensor_auth_bodies.cpp
Dawn/src/middleware/bap/activity_message/activity_sense_update_parser.cpp
Dawn/src/server/bap/encrypted/push/activity/activity_roster_push.cpp
Dawn/src/server/bap/encrypted/activity_message/patch_epoch/activity_patch_epoch_route.cpp
```

### 48.11 World and client hooks

```text
Dawn/src/client/hooks/bootflow/spawn_hold.cpp
Dawn/src/client/hooks/network/bubble_authority/bubble_authority_replacements.cpp
Dawn/src/client/hooks/network/bubble_authority/scope/bubble_authority_scope.cpp
Dawn/src/core/settings/client/definition.h
Dawn/src/core/settings/client/client_settings_parser.cpp
```

### 48.12 Case records

Use the case records for exact timestamps, handles, patches, and run conclusions.

```text
C:\Destiny 2 Development\HOMECOMING-FINDINGS.md
C:\Destiny 2 Development\HOMECOMING-HANDOFF.md
C:\Destiny 2 Development\HOMECOMING-MATCHMAKING-HANDOFF.md
C:\Destiny 2 Development\HOMECOMING-QOS-HANDOFF.md
C:\Destiny 2 Development\HOMECOMING-AH-HANDSHAKE-HANDOFF.md
C:\Destiny 2 Development\HOMECOMING-KIND22-HANDOFF.md
C:\Destiny 2 Development\HOMECOMING-AUTHORED-ROUTE-HANDOFF-20260819.md
C:\Destiny 2 Development\HOMECOMING-PHASE5-BUILDOUT.md
C:\Destiny 2 Development\DAWN-HOMECOMING-BRIEFING-FOR-CODEX.md
C:\Destiny 2 Development\OMEGA-PURPLE-VFX-HANDOFF-20260822.md
C:\Destiny 2 Development\OMEGA-MISSION-RECONSTRUCTION-COMPLETE-DOCUMENTATION-20260822.md
```

## 49. Handoff contract

An agent MUST provide a self-contained handoff after consequential work.

Use this schema:

```yaml
handoff:
  task: ""
  authorized_action: ""
  project_root: "C:\\Destiny 2 Development\\Dawn-src"
  branch: ""
  base_revision: ""
  worktree_state: ""
  executable_hash: ""
  built_dll_hash: ""
  deployed_dll_hash: ""
  settings: {}
  destination:
    package: ""
    activity: ""
    bubble: ""
    slice: ""
    arrival_hash: ""
    spawn_hash: ""
  expected_result: ""
  actual_result: ""
  highest_success_level: ""
  reconstruction_level: ""
  evidence_label: ""
  last_proved_layer: ""
  first_unproved_layer: ""
  exact_identities: []
  local_label_mappings: []
  confirmed_facts: []
  current_source_facts: []
  historical_facts: []
  inferred_facts: []
  hypotheses: []
  disproven_routes: []
  unknowns: []
  changed_files: []
  verification: []
  next_test:
    hypothesis: ""
    single_change: ""
    positive_result: ""
    negative_result: ""
    stop_condition: ""
  rollback: ""
```

### 49.1 Required context

Include process, build, source, connection, activity, group, and transition identities.

Include the exact settings used by the run.

### 49.2 Required evidence boundary

State the last proved layer.

State the first unproved layer.

Do not hide an unknown boundary inside a general summary.

### 49.3 Required negative evidence

List every controlled negative result that constrains the next agent.

Include the tested input and final consumer result.

### 49.4 Required next test

Provide only one next bounded test.

The test MUST include a stop condition and rollback.

### 49.5 Exact and local names

Mark recorder labels as local.

Provide complete native identities beside local labels.

Examples include `Scene 1`, `Candidate 8`, `identity2`, `BC30`, and `AAD0`.

## 50. Useful log queries

Set the log path once:

```powershell
$dawnLog = 'C:\Destiny 2 Development\bin\x64\Dawn\logs\dawn.log'
```

### 50.1 Process and build identity

```powershell
rg -n "process|build|hash|image|module|startup" $dawnLog
```

Confirm process and build identity before other interpretation.

### 50.2 Destination and activity session

```powershell
rg -n "service.?6|service.?7|destination|activity.?session|arrival|spawn.?set" $dawnLog
```

### 50.3 Matchmaking and advertisement

```powershell
rg -n "matchmaking|advertisement|search|locate|configuration|ambassador" $dawnLog
```

### 50.4 Gameplay group

```powershell
rg -n "peer|application.?ready|membership|parameter|activityHost|handoff|reestablish" $dawnLog
```

### 50.5 Activity messages

```powershell
rg -n "activity.?message|msg.?type|patch.?epoch|entity.?slot|bubble.?authority|state.?refresh" $dawnLog
```

### 50.6 Roster and runtime objects

```powershell
rg -n "roster|sensor.?auth|phase.?1|phase.?2|runtime.?object|registry|slot.?type|stateSequence" $dawnLog
```

### 50.7 Mission and sensors

```powershell
rg -n "mission|director|activity.?script|sense|trigger|selector|scalar|latch" $dawnLog
```

### 50.8 Scene and actor lifecycle

```powershell
rg -n "scene|cast|actor|entity.?factory|model|presentation|retire" $dawnLog
```

### 50.9 Effect provenance

```powershell
rg -n "effect|vfx|provider|transform.?bank|composite|renderer" $dawnLog
```

### 50.10 Failure view

```powershell
rg -n -i "fail|reject|invalid|mismatch|absent|timeout|drop|overflow|exhaust" $dawnLog
```

Do not interpret an isolated match without its correlation context.

Use a bounded time window around each match.

## 51. Non-regression gates

Complete the applicable gates before a completion claim.

### 51.1 Bootstrap gate

- BAP frames decode.
- Every request receives its defined response.
- Pending requests do not stall.
- Sensitive data does not enter logs.

### 51.2 Destination gate

- Service `6` request size is valid.
- Exact descriptor bytes remain preserved.
- Service `7` returns a nonzero session identifier.
- Forced destination does not alter unrelated routes.

### 51.3 World gate

- Package, bubble, and slice are correct.
- Spawn hash resolves.
- World reaches `arrived`.
- Spawn hold releases at the correct boundary.

### 51.4 Gameplay gate

- Association remains valid.
- Application readiness occurs.
- Reliable queues remain group-specific.
- View signature matches.

### 51.5 Activity-host gate

- Membership snapshot is complete.
- Local member reaches the required state.
- Group parameters publish in order.
- Activity-host identifier is nonzero.
- Manager postcondition is active.

### 51.6 Authority gate

- Global state is valid.
- Membership epoch is stable.
- Client and server slot sets are disjoint.
- Bubble authority uses a nonzero token.
- Fallback bubble `64` is never granted.

### 51.7 Mission gate

- Required runtime objects exist.
- Phase 2 bodies reach the correct generation.
- Mission seed occurs after arrival.
- Native trigger evidence exists.
- Mission owner advances.
- Successor state activates before completion.

### 51.8 Presentation gate

- Scene labels map to complete identities.
- Actors construct once.
- Model filters are exact.
- Effect ownership reaches the final consumer.
- World devices have presentation and collision proof.

### 51.9 State gate

- Revisions increase strictly.
- Prepared changes revalidate before commit.
- Notifications publish after commit.
- Connection-owned state resets by connection epoch.
- Native callbacks defer owning-state mutation.

### 51.10 Repeatability gate

- Two clean runs use the same baseline.
- Both runs reach the declared completion level.
- No experimental probe is required.
- Logs contain no new relevant failure.

## 52. Canonical glossary

Use one canonical term for each concept.

### Activity host

The service authority for one activity instance.

### Activity-host identifier

The identity used to locate or publish an activity host.

### Activity message

An activity-state message inside the BAP notification path.

### Activity script

A mission-control runtime object represented by Type `18` in recovered evidence.

### Activity session

The stable Dawn record for destination, membership, slots, and bubble authority.

### Advertisement

Public endpoint and activity data used to locate a host.

### Ambassador

The nonlocal identity that represents the public host or citizen path.

### Auth data

Authoritative object state published in phase 2.

### BAP

The bootstrap and account protocol used for secure services and activity messages.

### Biased field

A field whose encoded value includes a defined arithmetic offset.

### Bubble

A world-region index derived from a slice-set index.

### Bubble authority

Permission for a participant to own activity objects in one bubble.

### Cast

A scene-scheduled actor role.

### Client executor

A hook or native call that performs behavior inside the client process.

### Commit

The atomic application of a prepared state mutation.

### Component

A native runtime subsystem attached to a constructed object.

### Composite

A VFX runtime structure that combines provider data for a consumer.

### Descriptor

An exact opaque or partially decoded record used for destination or join state.

### Destination

The selected package, activity, world position, and descriptor state.

### DTLS

The protected transport layer inside gameplay association.

### Entity slot

A numbered allocation for an activity runtime object.

### Evidence boundary

The point between the last proved layer and the first unproved layer.

### Forced destination

A process-local override applied after normal destination selection.

### Generation

A value that separates reused native handles.

### Gameplay group

The peer membership and parameter session used for gameplay publication.

### Group host

The gameplay authority for one group session.

### Group parameter

One indexed value in the group parameter registry.

### Handle

A native runtime identity that can be reused after destruction.

### Host simulation

The preferred path where Dawn publishes state and the native client consumes it.

### Join descriptor

The exact `128` byte opaque matchmaking descriptor.

### Lifecycle edge

A native transition such as trigger, phase report, handoff, or retirement.

### Machine identifier

The identity for one network machine.

### Member key

The identity that binds a membership entry to its descriptor.

### Membership

The complete participant state for an activity or gameplay group.

### Mission director

A mission-control runtime object represented by Type `35` in recovered evidence.

### Mission graph

The complete set of mission phases, conditions, transitions, and successors.

### Native runtime object

An object that the Destiny 2 client factory constructs.

### Neutral body

A schema-valid body that produces an inactive or safe state.

### Patch epoch

A connection-owned version for activity patch publication.

### Phase 1

Roster registration, slot ownership, and group publication.

### Phase 2

Object-body publication for registered runtime objects.

### Postcondition

The stable state that must exist after an operation.

### Prepare

The validation and immutable construction before a state commit.

### Presentation

Visible or audible client output.

### Provider

A VFX object that selects or supplies transform data.

### Registry

A content collection that maps authored keys to native runtime definitions.

### Renderer input

The final data that the renderer consumes for visible output.

### Revision

A strictly increasing state version.

### Roster

The set of activity object groups and slot types for a scenario.

### RVA

A relative virtual address for one exact executable image.

### Scenario

The content record that associates destination state with bubbles and registries.

### Scene

An authored timeline with authority, casts, events, presentation, and lifecycle.

### Scene selector

The mission value that selects scene state.

### Sense data

Client-reported sensor input for activity objects.

### Slice

A world-state selection inside a bubble.

### SOID

A service object identity used in account and participation records.

### Spawn set

A catalog entry that resolves an arrival hash to spawn data.

### State sequence

The generation value for an auth and sense runtime object set.

### Transaction

A prepare, revalidate, commit, and publish state operation.

### Transform bank

A collection of candidate transforms for scene and effect systems.

### Transform provider

A runtime object that selects a transform or attachment socket.

### Transition token

A value that separates world-load state generations.

### VFX

A visual effect produced through authored events, providers, composites, and renderers.

### World device

A constructed portal, barrier, platform, or other interactive world object.

## 53. Final agent checklist

Before analysis:

- Confirm the user authorization.
- Confirm the source and deployed build identities.
- Confirm the active runtime settings.
- Identify the last proved layer.
- Identify the first unproved layer.

Before a change:

- Apply an evidence label.
- State one hypothesis.
- Define one independent input.
- Define positive and negative results.
- Define the stop condition.
- Define the rollback.
- Inspect overlapping user changes.

After a change:

- Verify the actual postcondition.
- Name the highest success level.
- Record negative evidence.
- Remove obsolete probes.
- Run applicable non-regression gates.
- Provide a self-contained handoff.

Final rule:

> Stop at the first unproved boundary. Change one owner. Verify the final consumer.
