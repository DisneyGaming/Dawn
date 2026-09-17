#pragma once
#include "../coo/lifecycle_service.h"
#include "../omega_ending_transit_rules.h"
namespace dawn::state::activity::strike_bond {
inline constexpr std::uint32_t kEndingMovieRegistry=0x15B232F8U,kEndingMovieSource=0x80F47449U;
inline constexpr std::uint32_t kEndingMovieResource=0x80C16F85U,kEndingMovieSelector=0x63769F9BU;
inline constexpr int kEndingRegion=25;
inline constexpr std::uint32_t kEndingSpawn=0xD49C610EU,kEndingWipe=0x2BA99C93U;
enum class EndingAnimationPhase : std::uint8_t { unavailable,flight,wipe,finished };
struct EndingActor {
    std::uint32_t entity{UINT32_MAX},serial{UINT32_MAX},controller{UINT32_MAX},selector{UINT32_MAX};
    bool valid() const noexcept {return entity!=UINT32_MAX && serial!=UINT32_MAX && controller!=UINT32_MAX && selector!=UINT32_MAX;}
    friend bool operator==(const EndingActor&,const EndingActor&)=default;
};
class EndingTransit {
    coo::Generation owner_{};std::uint64_t member_{};
    omega_ending_transit::Teleport host_{};bool bound_{},arrived_{},complete_{};
public:
    omega_ending_transit::Authority project(coo::Generation owner,std::uint64_t member,
        omega_ending_transit::Observation native) noexcept {
        if(!owner.valid() || !member || member==UINT64_MAX) return {};
        if(!bound_) {
            if(native.local.state!=0 || (!native.hasTeleport && native.local!=omega_ending_transit::Teleport{}))return {};
            auto token=static_cast<std::uint8_t>(native.local.token+1);if(!token)token=1;
            owner_=owner;member_=member;host_={1,token,kEndingRegion,kEndingSpawn};bound_=true;
        }
        if(owner!=owner_ || member!=member_)return {};
        const bool match=native.hasTeleport && native.local.token==host_.token
            && native.local.sliceSetIndex==kEndingRegion && native.local.sliceSetHash==kEndingSpawn;
        if(match && host_.state==1 && native.local.state==3 && native.hasRegion && native.currentRegion==kEndingRegion) {
            host_.state=3;arrived_=true;
        } else if(match && host_.state==3 && native.local.state==0) {host_.state=0;complete_=true;}
        return {host_,true,arrived_,complete_};
    }
};
// Host sequencing consumes native receipts; it never edits native ownership.
struct EndingFlow {
    coo::Generation owner{};EndingActor actor{};
    std::uint32_t revision{1},movieSelf{UINT32_MAX},resourceSelf{UINT32_MAX};
    std::uint64_t animationAt{};
    std::uint32_t wipeBiped{UINT32_MAX};
    bool closingReady{},wipePlaying{},wipeFinished{};
    bool wipe{},claimed{},animated{},retire{},retired{},arrived{},play{},started{},finished{};
    bool begin(coo::Generation token) noexcept {
        if(!token.valid() || (owner.valid() && owner!=token)) return false;
        owner=token;wipe=true;return true;
    }
    bool claim(coo::Generation token,EndingActor value) noexcept {
        if(token!=owner || !wipe || claimed || !value.valid()) return false;
        actor=value;claimed=true;return true;
    }
    bool animation(coo::Generation token,EndingActor value,bool active,std::uint64_t now) noexcept {
        if(token!=owner || !claimed || value!=actor || !active || animated) return false;
        animated=true;animationAt=now;return true;
    }
    bool playback(coo::Generation token,EndingActor value,std::uint32_t biped,EndingAnimationPhase phase) noexcept {
        if(token!=owner || !claimed || value!=actor || !animated || retire || wipeFinished
            || biped==UINT32_MAX || (wipeBiped!=UINT32_MAX && wipeBiped!=biped)) return false;
        if(phase==EndingAnimationPhase::wipe && !wipePlaying) {
            wipeBiped=biped;wipePlaying=true;return true;
        }
        // Record6 can reach its terminal idle only after the non-looping wipe
        // node. Require observing that node on this exact biped first.
        if(phase==EndingAnimationPhase::finished && wipePlaying) {wipeFinished=true;return true;}
        return false;
    }
    bool scene_cue(bool closing) noexcept {
        if(!owner.valid() || !animated || retire || !closing || closingReady) return false;
        closingReady=true;return true;
    }
    bool retire_due() const noexcept {return wipeFinished && closingReady;}
    bool retire_scene() noexcept {
        if(!owner.valid() || !retire_due()) return false;
        retire=true;return true;
    }
    bool cleanup(coo::Generation token) noexcept {
        if(token!=owner || !retire || retired) return false;
        retired=true;return true;
    }
    bool movie(coo::Generation token,std::uint32_t self,std::uint32_t resource,
               std::uint32_t nativeRevision,bool active) noexcept {
        if(token!=owner || !arrived || finished || self==UINT32_MAX || resource==UINT32_MAX
            || (movieSelf!=UINT32_MAX && movieSelf!=self)
            || (resourceSelf!=UINT32_MAX && resourceSelf!=resource)) return false;
        if(!play) {
            if(active) return false;
            movieSelf=self;resourceSelf=resource;play=true;return true;
        }
        if(nativeRevision!=revision) return false;
        if(active) {if(started)return false;started=true;return true;}
        if(!started) return false;
        play=false;finished=true;++revision;return true;
    }
};
}
