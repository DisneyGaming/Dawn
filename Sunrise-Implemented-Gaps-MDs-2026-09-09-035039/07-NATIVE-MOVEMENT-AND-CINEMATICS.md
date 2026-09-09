# Native movement and cinematic lifecycle observations

## Original gap

MISSING.md §10.5–10.6 distinguishes command acceptance from actual movement/playback. Our mapped native integrations observe the owner and its lifecycle rather than treating submission as completion. This note covers native mechanisms, not scripting or a mission walkthrough.

## Native movement bridge

The inspected bridge leaves placement, animation, effects, and movement with the native selector. It submits an authored selector action and observes its original lifecycle.

It resolves the current character and validates character/entity links. It then resolves the motion component, checks its self/entity identities, and locates the selector through the mapped native tuple. Tuple class information and the selector's links to the character/component must match.

Readable memory alone is not enough: a stale pointer or unrelated component must not count as the current selector. These checks depend on mapped layouts and definitions; they do not discover arbitrary motion controllers automatically.

Qualified animation/movement observations are forwarded to the current owner. Submission and departure-finished are distinct facts. Native code continues to execute the movement/path and animation.

## Native cinematic lifecycle

The documented ending integration separates retirement, destination/camera eligibility, playback, and completion. It requests roster retirement and waits for native retirement evidence before requesting the following region state. The retirement hook invokes the original native call and validates resulting state before acknowledging retirement.

The camera flow waits for the correct owner to be ready and inactive, publishes a play revision, observes actual active playback, then accepts inactive completion at the applicable revision. A supported skip requests a stop revision; skip input alone does not complete playback.

Callbacks copy qualified observations for the owning update path. Launch-queue acceptance and actual destination arrival also remain separate, so successful handoff bookkeeping is not automatically proof of arrival.

## Limits and evidence

These mechanisms close mapped cases, not the general actor-command-30 policy or every cinematic in MISSING.md. Historical accepted runs apply only to their documented candidates. No installed DLL or live playthrough was checked here.

Landmarks: Sunrise/src/client/hooks/bootflow/omega_mission_motion.inl. Native lifecycle documentation: Sunrise/docs/OMEGA-ENDING-TEARDOWN-TO-CUTSCENE.md and historical acceptance records in COO-EXECUTOR.md. These references identify where the evidence lives; the original documents and their scripting content are not included in this archive.
