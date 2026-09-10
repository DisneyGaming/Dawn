import test from 'node:test';
import assert from 'node:assert/strict';
import { createLaunchDriver } from './game_launch_driver.mjs';

function fixture() {
  let snapshot = 0, time = 0;
  const calls = [];
  const window = { app: 'process:D:\\Destiny3\\destiny2.exe', id: 17 };
  const sky = {
    list_apps: async () => [{ id: window.app, windows: [window] }],
    get_window: async () => window,
    get_window_state: async () => ({ window, screenshots: [{ id: String(++snapshot), width: 1920, height: 1080 }] }),
    press_key: async input => calls.push(input),
    click: async input => calls.push(input),
  };
  return { sky, calls, window, driver: createLaunchDriver(sky, { now: () => time }), setTime: value => { time = value; } };
}
test('normal input route requires fresh observations and completes once', async () => {
  const f = fixture();
  await f.driver.discover();
  let seen = await f.driver.observe();
  for (const stage of ['cover', 'character', 'orbit', 'director', 'mercury', 'launch']) {
    const previous = seen;
    seen = await f.driver.advance({ ...seen, stage, point: { x: 500, y: 400 } });
    await assert.rejects(f.driver.advance({ ...previous, stage: 'landed' }), /Stale/);
  }
  assert.deepEqual(f.calls.map(x => x.key ?? 'click'), ['Return', 'click', 'm', 'click', 'click', 'click']);
  assert.equal((await f.driver.advance({ ...seen, stage: 'loading' })).complete, false);
  seen = await f.driver.observe();
  assert.equal((await f.driver.advance({ ...seen, stage: 'landed' })).complete, true);
  await assert.rejects(f.driver.observe(), /stopped/);
});
test('ambiguous windows, out-of-bounds clicks, repeats and expired runs do not issue input', async () => {
  const f = fixture();
  f.sky.list_apps = async () => [{ id: f.window.app, windows: [f.window, f.window] }];
  await assert.rejects(f.driver.discover(), /found 2/);
  f.sky.list_apps = async () => [{ id: f.window.app, windows: [f.window] }];
  await f.driver.discover();
  let seen = await f.driver.observe();
  await assert.rejects(f.driver.advance({ ...seen, stage: 'character', point: { x: 1920, y: 5 } }), /Select/);
  seen = await f.driver.advance({ ...seen, stage: 'cover' });
  await assert.rejects(f.driver.advance({ ...seen, stage: 'cover' }), /did not advance/);
  assert.equal(f.calls.length, 1);
  f.setTime(16 * 60 * 1000);
  await assert.rejects(f.driver.advance({ ...seen, stage: 'character', point: { x: 5, y: 5 } }), /timed out/);
  assert.equal(f.calls.length, 1);
});
test('unknown input outcome invalidates the old screenshot', async () => {
  const f = fixture();
  await f.driver.discover();
  const seen = await f.driver.observe();
  f.sky.press_key = async () => { throw Error('user input detected'); };
  await assert.rejects(f.driver.advance({ ...seen, stage: 'cover' }), /outcome unknown/);
  await assert.rejects(f.driver.advance({ ...seen, stage: 'cover' }), /Stale/);
  assert.equal(f.driver.status().events.at(-1).type, 'input_or_refresh_unknown');
});
test('an already-landed session requires no menu inputs', async () => {
  const f = fixture();
  await f.driver.discover();
  const seen = await f.driver.observe();
  assert.equal((await f.driver.advance({ ...seen, stage: 'landed' })).complete, true);
  assert.equal(f.calls.length, 0);
});
