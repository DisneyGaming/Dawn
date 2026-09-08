-- Independent native profile: scene receipt, event-relative delay, marker, completion.
local root = graph("root", "chamber root", sequence(
    step("launch", "launch"),
    step("done", "done")
), {domain="composition", receipts={complete="done"}})
local room = graph("room", "chamber ending", sequence(
    step("scene", parallel("talk", "objective")),
    step("elapsed", command("timer", {id="authored_delay", argument=500})),
    step("complete", "finish")
), {receipts={["scene.started"]="talk", ["timer.elapsed"]="authored_delay"}})
return mission{
    id="chamber", graphs={root, room}, roles={mission="root", ending="room"},
    entry="root", modules={"room"}, observations={{fact="done", receipt="complete"}},
    presentation=presentation{markers={{objective="0x00000100", target="npc"}}},
}
