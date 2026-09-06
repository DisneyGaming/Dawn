#pragma once
#include "omega_adapter.h"
#include "../../../middleware/bap/activity_message/sensor_auth_update.h"

namespace sunrise::state::activity::coo::omega {
namespace wire = sunrise::middleware::bap::activity_message::sensor_auth_update;
// Field-for-field extraction from the accepted roster publisher. The caller
// selects the mission schema; this projection never changes codec selection.
inline void project(const Frame& frame, wire::Snapshot& snapshot) noexcept {
        const auto& presentation = frame.presentation;
        snapshot.omegaDialogueGenerations = presentation.generations;
        snapshot.omegaActiveDialogueRow = presentation.activeRow;
        snapshot.omegaObjectiveEvent = presentation.objective;
        snapshot.omegaIntroRevision = presentation.intro.revision;
        snapshot.omegaIntroPlay = presentation.intro.play;
        snapshot.omegaBossGeneration = presentation.bossGeneration;
        const auto& encounter = frame.encounter;
        snapshot.omegaFirstLairGeneration=encounter.generation;
        snapshot.omegaFirstLairLoose=encounter.loose;
        snapshot.omegaFirstLairAnchor=encounter.anchor;
        snapshot.omegaFirstCannonActive=encounter.cannon;
        snapshot.omegaFinalCannonActive=encounter.finalCannon;
        snapshot.omegaCrownRestricted=encounter.crownRestricted;
        snapshot.omegaCrownGeneration=encounter.crownGeneration;
        snapshot.omegaCrownLoose=encounter.crownLoose;
        snapshot.omegaCrownAnchor=encounter.crownAnchor;
        snapshot.omegaHiveLoose=encounter.hiveLoose;
        snapshot.omegaHiveAnchor=encounter.hiveAnchor;
        snapshot.omegaVexLoose=encounter.vexLoose;
        snapshot.omegaVexAnchor=encounter.vexAnchor;
        snapshot.omegaCabalLoose=encounter.cabalLoose;
        snapshot.omegaCabalAnchor=encounter.cabalAnchor;
        snapshot.omegaCrownCycle=encounter.cycle;
        snapshot.omegaCrownChargeEnabled=encounter.chargeEnabled;
        snapshot.omegaCrownChargeDunked=encounter.chargeDunked;
        snapshot.omegaCrownEyeStatusActive=encounter.eyeStatusActive;
        snapshot.omegaCrownReturnLaunch=encounter.returnLaunch;
        snapshot.omegaCrownTransitLaunches=encounter.transitLaunches;
        snapshot.omegaCrownTransitBridge=encounter.transitBridge;
        snapshot.omegaCrownTransitTarget=encounter.transitTarget;
        snapshot.omegaCrownTransitCreated=encounter.transitCreated;
        snapshot.omegaCrownFinalTraversal=encounter.finalTraversal;
        snapshot.omegaCrownRestriction=encounter.restriction;
        snapshot.omegaRescueSourcesGeneration=encounter.crownGeneration;
        snapshot.omegaRescueScenes=encounter.rescueScenes;
        snapshot.omegaRescueMarkerReadyMask=encounter.rescueMarkerReadyMask;
        const auto& ending = frame.ending;
        snapshot.omegaEndingRevision=ending.revision;
        snapshot.omegaEndingPlay=ending.play;
        snapshot.omegaEndingState=ending.bookendState?1U:0U;
        snapshot.omegaEndingRetire=ending.retireRoster;
        snapshot.omegaEndingSeedRuntime=wire::ending_runtime_seed_required(
            ending.bookendState,ending.arrived,ending.play,ending.started,ending.failed);
}
} // namespace sunrise::state::activity::coo::omega
