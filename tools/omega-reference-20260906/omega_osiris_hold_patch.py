"""Reapply the user-requested second Osiris hold alignment after an archive port."""

INCLUDE = '#include "../../../state/activity/omega_presentation.h"'
CALL = '''    const bool success = original(selector, entity, anchorParameter, transformOnly,
                                  actorBindings, result);'''
REPLACEMENT = '''    auto selectedAnchor = anchorParameter;
    // Both rescue Scenes use holding timeline 80EC0E00. Scene 9 anchors it to
    // ps_echo_converge_move_to (registry 99BD2FEB, type 48, slot 133).
    // Scene 27 exposes that same marker as parameter 0, but its hold node uses
    // parameter 2 (slot 134). Match the first hold without moving the entrance,
    // release animation, eye DPS, or the final-arena Scene 33 sharing this graph.
    if (call.accepts_side_effects() && entity == 0x80EC0E00U && anchorParameter == 2
        && safe_read<std::uint32_t>(selector, kInvalidHandle) == 0x80EC0DD4U
        && safe_read<std::uint32_t>(selector + 4U, kInvalidHandle) == 0x80806384U
        && omega_forced()) {
        namespace fight = state::activity::omega_first_lair;
        const auto nav = state::activity::omega_presentation::navigation();
        const auto status = fight::status(nav.run);
        const auto request = fight::scene_request(nav.run, 27);
        if (nav.enabled && nav.run != 0 && status.enabled && !status.failed
            && status.cycle == 2 && status.crownStage == fight::CrownStage::rescue
            && status.token.valid() && status.token.boss.run == nav.run
            && request.enabled && request.token == status.token
            && request.command.generation != 0 && !request.command.stop
            && request.command.eventCount == 0
            && state::activity::omega_rescue_npc::valid(request.command)) {
            selectedAnchor = 0;
        }
    }
    const bool success = original(selector, entity, selectedAnchor, transformOnly,
                                  actorBindings, result);
    if (selectedAnchor != anchorParameter && call.accepts_side_effects()
        && selector_capture_budget()) {
        report("ev=omega_osiris_hold stage=anchor slot=27 child=%08X "
               "anchor_param=%d selected_param=%d marker=133 success=%u",
               entity, anchorParameter, selectedAnchor, success ? 1U : 0U);
    }'''


def apply(text):
    if text.count(INCLUDE) != 1 or text.count(CALL) != 1:
        raise ValueError('Osiris hold patch requires the expected archive hook')
    return text.replace(INCLUDE, INCLUDE + '\n#include "../../../state/activity/omega_first_lair_runtime.h"').replace(CALL, REPLACEMENT)
