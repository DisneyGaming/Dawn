#pragma once

#include <cstdint>

namespace dawn::state::activity::omega_ending {

inline constexpr std::uint32_t kRegistry = 0x3A6CE17AU;
inline constexpr std::uint32_t kRegistryTag = 0x80F47BCEU;
inline constexpr std::uint32_t kDefinition = 0x80F47BCBU;
inline constexpr std::uint32_t kResource = 0x80C177DDU;
inline constexpr std::uint32_t kSelector = 0x2FECC6FDU;
inline constexpr std::uint32_t kEntity = 0x80C177DEU;
inline constexpr std::uint64_t kResourceOffset = 0x10578U;
inline constexpr std::uint16_t kSlot = 0;
inline constexpr std::uint8_t kBubble = 15;
inline constexpr std::uint8_t kState = 1;
inline constexpr std::uint16_t kSlice = 121;
/** Native player-point set in 80F50039, map-global Lighthouse32. Its three
 * authored points are within six metres of the bookend placement at height98.
 * The retail host's semantic set name remains unresolved. */
inline constexpr std::uint32_t kArrivalSpawnSet = 0xAB06CC27U;
/** Activity message 19 incident targets the client raises around the movie, as rows of
 * the SObject definition table (class 80807C9B, tag 80B9E5BF; all type code 17). The
 * FNV-1 name hashes resolve to `cinematic_started` (0xAECA3822), `cinematic_skip`
 * (0x7352DAFE) and `cinematic_finished` (0x3C03F059). The skip is a request to the host:
 * holding the prompt only raises it, and playback stops when the host answers with the
 * stop authority. */
inline constexpr std::uint32_t kCinematicStartedIncident = 5239;
inline constexpr std::uint32_t kCinematicSkipIncident = 3338;
inline constexpr std::uint32_t kCinematicFinishedIncident = 1685;

/** Identity from the encounter's qualified final death. Epoch distinguishes a
 * retry/checkpoint incarnation without relying on an elapsed fight duration. */
enum class Origin : std::uint8_t { encounter, preview };
struct Token final {
    std::uint64_t run{}, epoch{};
    std::uint32_t actor{UINT32_MAX}, generation{};
    Origin origin{Origin::encounter};
    [[nodiscard]] constexpr bool valid() const noexcept {
        if (origin == Origin::preview) {
            return run != 0 && epoch == 0 && actor == UINT32_MAX && generation == 0;
        }
        return origin == Origin::encounter && run != 0 && epoch != 0 && actor != UINT32_MAX && generation != 0;
    }
    friend constexpr bool operator==(const Token&, const Token&) = default;
};
enum class Phase : std::uint8_t { dormant, dialogue, preparing, offered, playing, complete, failed, retiring };
enum class Handoff : std::uint8_t { dormant, pending, claimed, queued, failed };
struct Authority final {
    Token token{};
    std::uint32_t revision{};
    bool bookendState{}, play{}, started{}, complete{}, failed{}, arrived{};
    /** Explicit zero-presence roster entries remain published through transit. */
    bool retireRoster{};
};

/** The letterboxed Lighthouse bookend lasts approximately 168 seconds. Native
 * type-6 playback owns all actors, speech and skips. Timers never report success. */
class Ending final {
public:
    [[nodiscard]] bool request(Token token, std::uint64_t now) noexcept {
        if (!token.valid() || token.origin != Origin::encounter) { return false; }
        if (phase_ != Phase::dormant) { return token_ == token; }
        token_ = token;
        // Intro and bookend have distinct native components; retain a nonzero
        // revision even after the run/epoch counters wrap their wire low bits.
        nextRevision_ = static_cast<std::uint32_t>((token.run * 32U) ^ (token.epoch * 2U)) | 1U;
        requestedAt_ = now;
        phase_ = Phase::dialogue;
        return true;
    }
    /** Explicit development preview from the opening Lighthouse. No Lair actors
     * have been admitted there, so there is no encounter retirement to await.
     * The normal host transit and native movie start/finish receipts still apply. */
    [[nodiscard]] bool request_preview(std::uint64_t run, std::uint64_t now) noexcept {
        if (run == 0 || phase_ != Phase::dormant) { return false; }
        token_ = {run, 0, UINT32_MAX, 0, Origin::preview};
        nextRevision_ = static_cast<std::uint32_t>(run * 32U) | 1U;
        requestedAt_ = now;
        stateRequested_ = true;
        retireRequested_ = true;
        retired_ = true;
        phase_ = Phase::preparing;
        return true;
    }
    [[nodiscard]] Phase phase() const noexcept { return phase_; }
    [[nodiscard]] Token token() const noexcept { return token_; }
    [[nodiscard]] Handoff handoff() const noexcept { return handoff_; }
    [[nodiscard]] Token handoff_request() const noexcept {
        return phase_ == Phase::complete && handoff_ == Handoff::pending ? token_ : Token{};
    }
    [[nodiscard]] bool claim_handoff(Token token) noexcept {
        if (!token.valid() || token != handoff_request()) { return false; }
        handoff_ = Handoff::claimed;
        return true;
    }
    [[nodiscard]] bool note_handoff_result(Token token, bool queued) noexcept {
        if (!token.valid() || token != token_ || handoff_ != Handoff::claimed) { return false; }
        handoff_ = queued ? Handoff::queued : Handoff::failed;
        return true;
    }
    [[nodiscard]] Authority authority() const noexcept {
        return {token_, revision_, stateRequested_, play_, started_,
                phase_ == Phase::complete, phase_ == Phase::failed, arrived_, retireRequested_};
    }
    [[nodiscard]] Token retirement_request() const noexcept {
        return phase_ == Phase::retiring ? token_ : Token{};
    }
    /** Host answer to the client's `cinematic_skip` incident: the same stop authority a
     * completion publishes, one revision ahead. The phase stays playing until the native
     * inactive receipt at that revision arrives, so completion keeps its single receipt path. */
    [[nodiscard]] bool skip() noexcept {
        if (phase_ != Phase::playing || !play_) { return false; }
        play_ = false;
        revision_ = nextRevision_++;
        return true;
    }
    /** Only a qualified native roster-removal receipt may release state121.
     * A sent/acknowledged packet and elapsed time are not cleanup receipts. */
    [[nodiscard]] bool observe_retirement(Token token, std::uint64_t now) noexcept {
        if (token != token_ || !token.valid() || !retireRequested_ || phase_ == Phase::failed) { return false; }
        if (retired_) { return true; }
        if (phase_ != Phase::retiring) { return false; }
        retired_ = true;
        stateRequested_ = true;
        phase_ = Phase::preparing;
        requestedAt_ = now;
        return true;
    }
    /** Caller supplies a native current-region receipt for the exact completed
     * host teleport transaction. Selecting an advertisement is not arrival. */
    [[nodiscard]] bool observe_arrival(Token token, std::int32_t region) noexcept {
        if (token != token_ || !token.valid() || !stateRequested_ || region != kSlice) { return false; }
        arrived_ = true;
        return true;
    }
    void advance(std::uint64_t now, bool finalDialogueFinished) noexcept {
        if (phase_ == Phase::dialogue && finalDialogueFinished) {
            phase_ = Phase::retiring;
            retireRequested_ = true;
            requestedAt_ = now;
        }
        if ((phase_ == Phase::dialogue || phase_ == Phase::retiring
             || phase_ == Phase::preparing || phase_ == Phase::offered)
            && now >= requestedAt_ && now - requestedAt_ >= 120000U) { fail(); }
        if (phase_ == Phase::playing && now >= startedAt_ && now - startedAt_ >= 300000U) { fail(); }
    }
    void observe(Token token, std::uint32_t revision, bool active, bool resourceReady,
                 std::uint64_t now) noexcept {
        if (token != token_ || !token.valid()) { return; }
        if (phase_ == Phase::preparing && arrived_ && resourceReady && !active) {
            revision_ = nextRevision_++;
            play_ = true;
            phase_ = Phase::offered;
            offeredAt_ = now;
            ++attempts_;
        } else if (phase_ == Phase::offered && revision == revision_) {
            if (active && resourceReady) {
                phase_ = Phase::playing;
                startedAt_ = now;
                started_ = true;
            } else if (!active && now >= offeredAt_ && now - offeredAt_ >= 1000U) {
                if (attempts_ >= 3U) { fail(); }
                else { phase_ = Phase::preparing; }
            }
        } else if (phase_ == Phase::playing && revision == revision_ && !active && resourceReady) {
            // The accepted native active latch must be observed before an
            // inactive receipt can end the fight. An unloaded resource cannot.
            phase_ = Phase::complete;
            handoff_ = Handoff::pending;
            play_ = false;
            revision_ = nextRevision_++;
        }
    }
private:
    void fail() noexcept {
        phase_ = Phase::failed;
        play_ = false;
        revision_ = nextRevision_++;
    }
    Token token_{};
    Phase phase_{};
    Handoff handoff_{};
    std::uint32_t revision_{}, nextRevision_{1};
    std::uint8_t attempts_{};
    std::uint64_t requestedAt_{}, offeredAt_{}, startedAt_{};
    bool stateRequested_{}, play_{}, started_{}, arrived_{};
    bool retireRequested_{}, retired_{};
};

} // namespace dawn::state::activity::omega_ending
