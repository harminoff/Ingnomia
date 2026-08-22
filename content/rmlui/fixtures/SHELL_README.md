# Outer-shell fixture and evidence boundary

The registered Wave 3 documents are under `documents/`, `screens/`, and
`modals/`. They intentionally contain stable element IDs but no authored action
names or inline event strings. `ShellRmlAdapter` maps those IDs to a closed C++
callback enum; `ShellController` alone creates typed action envelopes.

Dependency-free checks:

```powershell
cmake -S tests/ui-shell -B build/ui-shell-tests
cmake --build build/ui-shell-tests --config Release
ctest --test-dir build/ui-shell-tests -C Release --output-on-failure
```

For literal isolated rendering, stage the complete `content/rmlui` tree beside
the already-built Qt/RmlUi spike, copy one registered screen to the spike's
`spike.rml`, and run its `--self-test` capture. This proves that one document
renders in the isolated host. It does not prove full-game routing, aggregator
wiring, a Steam-blocked world lifecycle, or final focus/input behavior.
