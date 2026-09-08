"""Build/test the Lua authoring migration in isolated output; never install or launch."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime
import json
import os
from pathlib import Path
import verify

ROOT = verify.ROOT
DEFAULT_OUT = ROOT / ("build/coo/validation-lua-" + datetime.now().strftime("%Y%m%d-%H%M%S"))
TESTS = (
    "coo_lua_tests", "deadly_trial_tests", "coo_mission_script_tests",
    "coo_script_tests", "coo_universal_services_tests", "coo_executor_tests",
    "coo_shared_tests", "gateway_opening_tests", "player_position_tests",
    "coo_opening_tests", "coo_forest_tests", "coo_forest_runtime_tests",
    "coo_combat_tests", "coo_combat_runtime_tests", "coo_ending_tests",
    "coo_ending_runtime_tests",
)

def source_manifest():
    """Files that define the DLL, Lua missions, regression tests, and delivery tools."""
    paths = set()
    for folder in ('Sunrise/src', 'Sunrise/scripts', 'Sunrise/unit', 'Sunrise/vendor', 'Sunrise/resources', 'Sunrise/docs', 'tools/coo'):
        for path in (ROOT / folder).rglob('*'):
            complete_tree = folder in ('Sunrise/src', 'Sunrise/scripts', 'Sunrise/vendor', 'Sunrise/resources') or path.is_relative_to(ROOT / 'Sunrise/unit/fixtures')
            if path.is_file() and (complete_tree or path.suffix.lower() in ('.cpp', '.h', '.c', '.hpp', '.inl', '.vcxproj', '.props', '.lua', '.py', '.ps1', '.md', '.txt', '.rc', '.ico')):
                paths.add(path)
    paths.update((ROOT / 'Sunrise/Sunrise.vcxproj', ROOT / 'Sunrise/lua-items.props'))
    return {p.relative_to(ROOT).as_posix(): verify.digest(p) for p in sorted(paths)}


def check(name, configuration):
    project = ROOT / ("Sunrise/Sunrise.vcxproj" if name == "Sunrise" else f"Sunrise/unit/{name}.vcxproj")
    result = verify.build(project, configuration)
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    parser.add_argument("--project", action="append", choices=(*TESTS, "Sunrise"))
    parser.add_argument("--configuration", action="append", choices=("Debug", "Release"))
    parser.add_argument("--tests-only", action="store_true")
    args = parser.parse_args()
    # Large native data tables exceed the 32-bit compiler process heap in Debug.
    os.environ["PreferredToolArchitecture"] = "x64"
    amd64 = verify.MSBUILD.parent / "amd64/MSBuild.exe"
    if amd64.is_file():
        verify.MSBUILD = amd64
    verify.OUT = args.out.resolve()
    if not verify.OUT.is_relative_to((ROOT / "build/coo").resolve()):
        raise SystemExit("Validation output must stay under build/coo.")
    if (verify.OUT / "installation.json").exists() or (verify.OUT / "package.json").exists():
        raise SystemExit("Preserve installed-build evidence; choose a new --out directory.")
    verify.OUT.mkdir(parents=True, exist_ok=True)
    names = args.project or list(TESTS) + ([] if args.tests_only else ["Sunrise"])
    configurations = args.configuration or ("Debug", "Release")
    jobs = [(name, conf) for name in names for conf in configurations
            if f'Include="{conf}|x64"' in (ROOT / ("Sunrise/Sunrise.vcxproj" if name == "Sunrise" else f"Sunrise/unit/{name}.vcxproj")).read_text()]
    full_run = not args.project and not args.configuration and not args.tests_only
    before = source_manifest() if full_run else None
    if before is not None:
        (verify.OUT / 'source-manifest.json').write_text(json.dumps(before, indent=2))
    results = []; failures = []
    with ThreadPoolExecutor(max_workers=2) as pool:
        pending = {pool.submit(check, *job): job for job in jobs}
        for future in as_completed(pending):
            job = pending[future]
            try:
                result = future.result(); results.append(result)
                if "dll" in result:
                    print(f"Sunrise {job[1]} candidate: {result['sha256']}", flush=True)
            except Exception as exc:
                failures.append({"project": job[0], "configuration": job[1], "error": str(exc)})
                lines = str(exc).splitlines()
                relevant = [line for line in lines if any(word in line.lower() for word in ("error", "warning", "fail"))]
                detail = "\n".join(relevant or lines[-12:])
                print(f"FAILED {job[0]} {job[1]}: {detail}", flush=True)
            (verify.OUT / "results.json").write_text(json.dumps(results, indent=2))
            (verify.OUT / "failures.json").write_text(json.dumps(failures, indent=2))
    if before is not None and source_manifest() != before:
        failures.append({'project': 'source', 'error': 'Source changed during validation; rerun against stable source.'})
        (verify.OUT / 'failures.json').write_text(json.dumps(failures, indent=2))
    if failures:
        raise SystemExit(1)
    print(f"PASS: {len(results)} builds/tests; candidate only, no installation", flush=True)

if __name__ == "__main__":
    main()
