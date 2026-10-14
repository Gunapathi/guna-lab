# Handover

> **Owns:** where the work stands at the end of the last session, and how the next one picks up:
> what's untested, the owner's next steps, open decisions, traps found on the way.
> A snapshot: rewrite it at the end of the next long session.
> **Does not own:** what happened when ([log.md](log.md)), the hardware checklists
> ([testing.md](testing.md)), the milestones ([roadmap.md](roadmap.md)), any rule (each lives in
> its doc, [index.md](../index.md)).

**Session:** 2026-10-14, Claude Code in VS Code on the owner's PC.

---

## Where it stands

- **Firmware 0.1.0, protocol v1:** M0–M2 built in `firmware/`. It builds for both boards, and
  the 23 unit tests pass on the PC. **Nothing has run on hardware.**
- **Not a git repository yet,** so nothing is committed. The guard works without git. Its
  AI-trailer step starts once the owner runs `git init`.
- **The app (M4):** not started; the decision is the owner's ([app.md](../features/app.md#the-decision)).
- **`ROVER_PLAN.md`** is untouched. `spec/` took it over, and the differences are in
  [log.md](log.md#2026-10-14-the-c3-firmware-m0m2-and-the-spec).

## Installed on this PC this session

- **PlatformIO Core 6.2.0** in `%USERPROFILE%\.platformio\penv`, plus the `espressif32` 7.1.3
  platform, the RISC-V toolchain and the Arduino framework (about 1 GB under
  `%USERPROFILE%\.platformio`). The PlatformIO IDE extension will reuse it.
- **Nothing on the system PATH.** The unit tests ran with PlatformIO's MinGW package in a
  temporary folder; for your own runs, see [build.md](../core/build.md#unit-tests-on-the-pc).

## The owner's next steps, in order

1. In VS Code: install **PlatformIO IDE**, then **File → Open Workspace from File →
   `rover-c3.code-workspace`**.
2. Optional, for `pio test -e native` and the guard's last step: a host `gcc`
   ([build.md](../core/build.md#unit-tests-on-the-pc)).
3. The hardware pass: M0 → M1 → M2 in [testing.md](testing.md), with
   [bluefruit-test.md](../guides/bluefruit-test.md) for the phone steps.
4. Decide the app: A (a new Flutter app, recommended) or B (from Machi's app).
5. `git init` when you want history; then commit (the guard first: `python tool/guard.py`).

## Traps found on the way

- PlatformIO's `espressif32` is at 7.x now, but it still ships Arduino core 2.0.17. Don't assume
  7.x means core 3.x.
- Old MinGW's `snprintf` returns −1 on a cut line, so RoverCore doesn't use the return value.
- In Git Bash, a PATH entry must be `/c/...`, not `C:/...` (the colon splits it).
- `pio pkg list` needs `PYTHONIOENCODING=utf-8` in this console.
