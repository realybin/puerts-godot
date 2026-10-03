# Test Runner

Run from the repository root:

```powershell
python scripts/run_tests.py --godot C:\path\to\Godot_v4.x.x-stable_win64_console.exe --timeout=80
```

`--backends quickjs,lua` limits the backends. `--timeout` limits one run; a backend that keeps Godot alive is terminated after shutdown grace time. The runner syncs `bin/` to `tests/bin/` and starts `tests/main.tscn`.

Runtime tests live in `tests/tests/`; each `test_*.gd` extends [`support/test_case.gd`](../tests/tests/support/test_case.gd). Test methods take no arguments and return `bool`. Use `new_environment()` for setup; the runner disposes environments after each test. `equal()`, `check()`, and `skip()` record results.
