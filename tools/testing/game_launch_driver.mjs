// Run from Codex's node_repl with @oai/sky. The agent inspects each screenshot
// before naming the stage and selecting a target. No fixed-delay click replay.
export const launchStages = Object.freeze([
  'cover', 'character', 'orbit', 'director', 'mercury', 'launch', 'loading', 'landed',
]);
const inputFor = Object.freeze({
  cover: { key: 'Return' },
  character: { target: 'Titan (middle character)' },
  orbit: { key: 'm' },
  director: { target: 'Mercury destination' },
  mercury: { target: 'Lighthouse landing zone' },
  launch: { target: 'Launch button' },
});

export function createLaunchDriver(sky, {
  app = 'process:D:\\Destiny3\\destiny2.exe',
  now = () => Date.now(),
  maximumDurationMs = 15 * 60 * 1000,
  maximumInputs = 8,
} = {}) {
  if (!sky || !Number.isFinite(maximumDurationMs) || maximumDurationMs <= 0
      || !Number.isInteger(maximumInputs) || maximumInputs < 1 || maximumInputs > 32) {
    throw new Error('Invalid launch driver configuration');
  }
  let target, latest, sequence = 0, started, inputs = 0, lastStage = -1, stopped = false;
  const events = [];
  const record = (type, details = {}) => events.push({ time: now(), type, ...details });
  function active() {
    if (stopped) throw new Error('Driver stopped; create a new driver for another launch');
    if (started !== undefined && now() - started > maximumDurationMs) {
      stopped = true;
      record('timeout');
      throw new Error('Launch timed out; inspect the game and log before restarting');
    }
  }
  function identity(window) {
    return window && target && window.id === target.id && window.app === target.app;
  }
  async function discover() {
    active();
    if (target) throw new Error('Already bound; use a new driver after a process restart');
    const apps = await sky.list_apps();
    const matches = apps.filter(item => item.id === app).flatMap(item => item.windows);
    if (matches.length !== 1) throw new Error(`Expected one game window; found ${matches.length}`);
    // Only use window identity returned by the Computer Use API.
    target = await sky.get_window({ id: matches[0].id, app: matches[0].app });
    if (target.id !== matches[0].id || target.app !== matches[0].app) {
      target = undefined;
      throw new Error('Window identity changed during selection');
    }
    started = now();
    record('bound');
    return target;
  }
  async function observe() {
    active();
    if (!target) throw new Error('Discover the game window first');
    latest = undefined;
    const state = await sky.get_window_state({ window: target, include_screenshot: true, include_text: false });
    if (!identity(state.window)) throw new Error('Window identity changed; stop and rediscover');
    target = state.window;
    const screenshot = state.screenshots?.[0];
    if (!screenshot?.id) throw new Error('No screenshot; do not issue menu input');
    latest = { sequence: ++sequence, screenshotId: screenshot.id, state };
    return { sequence, screenshotId: screenshot.id, state };
  }
  async function advance({ sequence: observedSequence, screenshotId, stage, point } = {}) {
    active();
    if (!latest || observedSequence !== latest.sequence || screenshotId !== latest.screenshotId) {
      throw new Error('Stale observation; capture and inspect the game again');
    }
    const index = launchStages.indexOf(stage);
    if (index < 0 || index <= lastStage) throw new Error('Stage did not advance; observe instead of repeating input');
    if (stage === 'loading' || stage === 'landed') {
      latest = undefined;
      lastStage = index;
      record(stage);
      if (stage === 'landed') stopped = true;
      return { stage, complete: stage === 'landed' };
    }
    const action = inputFor[stage];
    const screenshot = latest.state.screenshots[0];
    if (action.target && (!point || !Number.isFinite(point.x) || !Number.isFinite(point.y)
        || point.x < 0 || point.y < 0
        || (screenshot.width !== undefined && point.x >= screenshot.width)
        || (screenshot.height !== undefined && point.y >= screenshot.height))) {
      throw new Error(`Select ${action.target} from the current screenshot`);
    }
    if (inputs >= maximumInputs) throw new Error('Input limit reached; inspect before starting a new run');
    const observation = latest;
    latest = undefined;
    ++inputs;
    record('input_attempt', { stage, input: action.key ?? action.target });
    try {
      if (action.key) await sky.press_key({ window: observation.state.window, key: action.key });
      else await sky.click({ window: observation.state.window, screenshotId,
        x: point.x, y: point.y });
      lastStage = index;
      // Refresh immediately. The caller must inspect this result in a separate
      // model/tool turn before choosing another input.
      const refreshed = await observe();
      record('input_returned', { stage });
      return refreshed;
    } catch (error) {
      latest = undefined;
      record('input_or_refresh_unknown', { stage, reason: String(error.message ?? error) });
      throw new Error('Input or refresh outcome unknown; reobserve before deciding what to do', { cause: error });
    }
  }
  function stop() { latest = undefined; stopped = true; record('stopped'); }
  function status() {
    return { inputs, lastStage: launchStages[lastStage] ?? null, stopped,
      events: events.map(event => ({ ...event })) };
  }
  return Object.freeze({ discover, observe, advance, stop, status });
}
