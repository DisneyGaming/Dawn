-- Independent profile with a forward-referenced fork/join and custom receipts.
local launch = graph("launch", "Pump station setup", {
    step("finish", "confirmed", {after={"wait_pump", "wait_display"}}),
    step("wait_display", "display_ready", {after={"publish"}}),
    step("wait_pump", "pump_ready", {after={"publish"}}),
    step("publish", parallel("display", "pump")),
}, {
    domain="composition",
    receipts={
        ["pump.running"]="pump_ready",
        ["display.running"]="display_ready",
        ["operator.confirmed"]="confirmed",
    },
})
local room = graph("room", "Independent scene and population", {
    step("camera", "scene"),
    step("actors", "population"),
}, {
    receipts={["scene.ready"]="scene", ["population.finished"]="population"},
})
return mission{
    id="pump_station",
    graphs={launch, room}, roles={encounter="room"}, entry="launch",
    modules={"display", "pump"},
    observations={
        {fact="operator_confirmed", receipt="operator.confirmed"},
        {fact="pump_running", receipt="pump.running"},
        {fact="display_running", receipt="display.running"},
    },
    presentation=presentation{
        dialogue={dispatch_timeout_ms=100, spacing_ms=5},
        cue_sets={signals={
            {event="power_on", cycles=array{}, actions={
                {operation="dialogue", value=0, delay_ms=3},
            }},
        }},
        action_sets={mission_success={
            {operation="objective", value=1280, delay_ms=0},
        }},
    },
}
