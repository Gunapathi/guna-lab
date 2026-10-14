# Guards: what runs before a commit, and why

> **Owns:** what `tool/guard.py` checks, why each check exists, what it can't catch, and the
> commit rules.
> **Does not own:** the rules being checked: the protocol ([protocol.md](protocol.md)), the
> layering ([architecture.md](architecture.md)), the definition of done
> ([build.md](build.md#definition-of-done)).

---

## How to run

```powershell
python tool/guard.py          # all 7 steps (the two board builds take a minute or two)
python tool/guard.py --fast   # steps 1-5 only, about a second: use it while iterating
```

`READY TO COMMIT` (exit 0) or `BLOCKED` (exit 1, with every problem listed). A step that can't
run (no PlatformIO, no `gcc`) says `SKIPPED` and why; the verdict then says how many steps were
skipped, so a skip is never mistaken for a pass.

- It checks the working tree as it is, so it works before the folder is a git repository.
- It's a command, not a git hook: hooks aren't carried by a clone, so a hook silently goes
  missing. With a command, no output means it wasn't run, never that it passed.
- `PIO` (environment variable) overrides where it looks for `pio`.

## What runs

| # | Step | Blocks when | Why |
|---|---|---|---|
| 1 | Code map | A source file under `firmware/src`, `include`, `lib` or `test` (and `app/lib` once it exists) isn't named in `spec/code-map.md` | The map is how anyone finds code without re-reading the tree; a stale map is worse than none |
| 2 | Doc links | A markdown link or `#anchor` in `CLAUDE.md` or `spec/**` doesn't resolve | Docs link instead of repeating rules; a broken link loses the rule |
| 3 | Protocol constants | `PROTOCOL_VERSION` in `config.h` isn't the **Version** in [protocol.md](protocol.md), or a NUS UUID in `config.h` isn't in protocol.md | The doc is the contract; firmware that drifts from it breaks the app silently |
| 4 | RoverCore purity | A file in `firmware/lib/RoverCore` includes Arduino, NimBLE, FreeRTOS or ESP-IDF headers, or `config.h` | RoverCore must build on the PC and take tunables as parameters ([architecture.md](architecture.md#rovercore-the-pure-c-core)) |
| 5 | AI trailers | In a git repository: a commit message names an AI as co-author or generator | Commit rules below |
| 6 | Firmware build | `pio run` fails for `supermini` or `xiao` | Definition of done; one firmware serves both boards |
| 7 | Unit tests | `pio test -e native` fails | Definition of done; the failsafes and the protocol are proved here before any hardware |

## What these do NOT catch

- **Behaviour on the rover.** Nothing here knows whether the tracks turn the right way, or
  whether a hard start resets the ESP32; that's the hardware checklist in
  [testing.md](../status/testing.md).
- **Step 1 works by file name**, so two files with the same name in different folders count as
  one.
- **Step 3 checks the version and the UUIDs only**, not every packet's layout; the unit tests'
  vectors cover those.

## Commit rules

- **The owner commits.** Agents never commit or stage, and never push.
- **Author** = the owner's local git identity. **No `Co-Authored-By`, "Generated with" or any
  other AI attribution** in any commit message (step 5 blocks it).
- **Message style:** `type(area): what changed`, where type is `feat` / `fix` / `docs` /
  `chore` / `test`, and area is `firmware`, `app`, `spec` or `tool`.
