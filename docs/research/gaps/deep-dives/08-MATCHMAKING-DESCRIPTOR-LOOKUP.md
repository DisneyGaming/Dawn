# State-backed matchmaking descriptor lookup

## Original gap

MISSING.md §0.1 describes matchmaking replies as empty or ID-bearing shapes. Our locate-session path can return a retained advertisement descriptor. This is a lookup capability, not a queue or general search service.

## Advertisement preparation

The route parses the request kind. An advertisement update supplies an existing ID, variant key, descriptor-presence flag, and descriptor bytes to state preparation.

Preparation requires exactly the supported descriptor size when present and no stray bytes when absent. It resolves the logical context under the state lock and refuses invalid contexts or exhausted revisions.

ID selection has explicit precedence: request-supplied ID, otherwise an existing variant ID, otherwise the next available allocator ID. Preparation records expected context/allocator revisions in a pending mutation. Parsing a valid request does not by itself change stored state.

## Lookup and encoding

A locate-session request acquires the context's latest snapshot. Its advertisement ID enters the reply, and its descriptor is passed to the encoder when present. The snapshot stays alive through encoding and is erased afterwards.

The route then reports whether a pending mutation exists for the surrounding transaction to handle. Failed state-field preparation clears the mutation and uses an empty fallback. Failed encoding clears the mutation, resets the written length, and returns failure.

This separates retained advertisement state, projected output, and eventual state commit. An output-construction failure does not become a successful prepared mutation.

## What this adds and what it does not

The client can receive actual descriptor bytes retained for the logical context, rather than only a placeholder ID. The underlying state is in-memory; this is not evidence of disk persistence.

It does not implement candidate discovery, ranking, multiplayer queues, population matching, QoS probing, NAT traversal, or host handoff. Other request kinds can still use static shapes. The comparison should mark descriptor lookup as partial progress, not full matchmaking completion.

Landmarks: Dawn/src/server/bap/encrypted/matchmaking/matchmaking_route.cpp; Dawn/src/state/matchmaking/transactions/matchmaking_prepare.cpp, with supporting commit/state and codec files in those matchmaking directories. Route and preparation bodies were inspected; no live matchmaking test was run.
