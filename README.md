# pRedirectWaypoint

A shoreside MOOS-IvP dispatcher app implementing a **click-to-redirect** workflow in pMarineViewer for Aquaticus capture-the-flag (CTF) and high-value-target (HVT) missions.

The operator selects a team and a boat, then left-clicks anywhere on the map to send that boat to the clicked point. On arrival, the boat station-holds at the clicked position until redirected again or the team is released. Only boats you actually engage (select) are driven — their manual override is dropped and they **stay under power until the team is released**, so a boat finishes its waypoint and holds at the destination even after you select off it. The rest of a controlled team, and the other team, are untouched.

`pRedirectWaypoint` is a thin dispatcher: it performs **no geodesy and owns no behavior logic**. It only maps operator intent (team/boat selection + map clicks) onto per-vehicle command variables (`REDIR_CONTROLLED_<VNAME>`, `REDIR_ACTIVE_<VNAME>`, `REDIR_UPDATE_<VNAME>`) that `uFldShoreBroker` qbridges down to each vehicle, where the `station_hold` and `waypt_redirect` behaviors act on them.

---

## Operating Model

The keys below are `hotkey` lines in the mission's pMarineViewer block
(the fork's viewer binds nothing by itself):
`hotkey = b # RDR_SEL_TEAM=blue`, `hotkey = r # RDR_SEL_TEAM=red`,
`hotkey = 1-9 # RDR_SEL_INDEX=$[KEY]`, `hotkey = w # RDR_CLICK_MODE_TOGGLE`,
`hotkey = u # RDR_UNTAG_TOGGLE`.

| Input | Action |
|---|---|
| `DEPLOY` (existing) | Boats deployed as usual. |
| `b` / `r` | Select the **click-target team** (blue/red). Resets the boat index. Selection alone does *not* change control. |
| `w` (key or on-screen button) | Toggle **control** for the currently selected team. A controlled team becomes clickable; boats you select on it come under power and stay driving until release. Releasing returns each engaged boat to its pre-control state (parked if not deployed, autonomous if deployed). Multiple teams can be controlled at once. |
| `1` / `2` / `3` ... | Select which boat on the click-target team the next click commands (1-based roster index). |
| left-click | Send the selected boat to the clicked point (`REDIR_ACTIVE` + `REDIR_UPDATE`). On arrival the boat reverts to station-hold *at the clicked point*. |
| `u` (or on-screen button, `RDR_UNTAG_TOGGLE`) | Per-boat: allow/disallow the selected boat to auto-return home to untag while controlled. Default OFF (HVT "tagged = dead"); ON gives CTF-style recovery. |

Notes on the control model:

- **Controlled == held + clickable.** A click only acts if the selected team is currently in the controlled set; otherwise it is ignored (with a warning in the appcast).
- Pressing `w` on a newly controlled team clears any stale click-waypoint so boats hold at their *current* position rather than resuming an old redirect.
- Switching the click target between two already-controlled teams does not release either team or cancel an in-flight waypoint.
- Releasing a team clears its click-waypoints, arrival latches, and untag-allow flags.
- **Only engaged boats are woken, and they stay awake until release.** Taking control of a team no longer drops manual override on every boat of that team (which made the whole team go live as if `DEPLOY` had been pressed). A boat's override is dropped when you **select** it on a controlled team; it then holds position, and on a click drives to the point and station-holds there. Selecting off it (or onto another boat) does **not** park it — it keeps its waypoint and finishes. Boats you never select stay parked.
- **Release restores the pre-control state.** Pressing `w` to release a team returns each boat this app woke to how it was before you took it: **parked** (`MOOS_MANUAL_OVERRIDE=true`) if `DEPLOY` was never pressed, or **running its autonomous behaviors** (override off) if the game is deployed. The dispatcher tracks `DEPLOY_ALL` to decide this. Boats it never woke are never overridden, so an autonomously-deployed vehicle is never frozen. On release it also erases the boat's `station_hold` keep-in/out/hiber polygons — forcing manual override parks the helm before `BHV_StationKeep` can clear its own visuals, so the dispatcher removes them explicitly.
- *(Experimental)* After a redirect completes, the vehicle-side `constant_heading_redirect` behavior is intended to point the boat on a commanded heading while `station_hold` keeps position. This feature is **not fully implemented or tested** — no heading publisher exists yet. The dispatcher already clears the `REDIR_ARRIVED` latch on every new click, release, and new control session so the behavior can never fight an in-transit waypoint with stale state.

## On-Screen Feedback

- **Control circles:** every boat on a controlled team gets a magenta `VIEW_CIRCLE` halo redrawn each tick at its live position (from `NODE_REPORT`s). The currently selected boat's circle is filled; others are outline-only. Tagged boats lose their halo. Circles are cleared when a team is released.
- **Appcast report:** controlled teams, click target, selected boat, rosters, untag-allow list, click/waypoint counters, last target/point, warnings, and (with `debug = true`) a timestamped rolling debug log.
- **`RDR_CLICK_MODE` latch:** published `true` whenever any team is controlled — handy as a pMarineViewer `scope` field.

## MOOS Interface

### Subscriptions

| Variable | Type | Meaning |
|---|---|---|
| `RDR_SEL_TEAM` | string | `blue` \| `red` — sets the click-target team |
| `RDR_SEL_INDEX` | string/double | `1..N` — boat index on the click-target team |
| `RDR_CLICK_MODE_TOGGLE` | string | changing value (edge) — toggles control for the selected team |
| `RDR_UNTAG_TOGGLE` | string | changing value (edge) — flips untag-allow for the selected boat |
| `RDR_CLICK` | string | `x=<m>,y=<m>` (local grid) — the map click |
| `NODE_REPORT`, `NODE_REPORT_*` | string | live positions for control-circle drawing |
| `TAGGED_VEHICLES` | string | authoritative CSV of tagged boats |
| `TAGGED_<VNAME>` | string | per-boat tag state |

### Publications

| Variable | Type | Meaning |
|---|---|---|
| `RDR_CLICK_MODE` | string | `true` \| `false` latch — any team controlled |
| `REDIR_CONTROLLED_<VNAME>` | string | `true` \| `false` — hold + suppress game play |
| `REDIR_ACTIVE_<VNAME>` | string | `true` \| `false` — activates `waypt_redirect` |
| `REDIR_UPDATE_<VNAME>` | string | `points=x,y#capture_radius=..#slip_radius=..#speed=..` |
| `REDIR_ALLOW_UNTAG_<VNAME>` | string | `true` \| `false` — per-boat auto-untag permission |
| `REDIR_ARRIVED_<VNAME>` | string | clear-only (`false`) — resets the arrival latch on each new click, team release, and new control session. Set `true` on the vehicle by the `waypt_redirect` endflag; gates the *experimental* post-arrival `constant_heading_redirect` behavior |
| `VIEW_CIRCLE` | string | control halos for pMarineViewer |

All variable names are overridable in the config block (see below).

## Example Configuration

```
ProcessConfig = pRedirectWaypoint
{
  AppTick   = 4
  CommsTick = 4

  // Ordered rosters. List position == selection index (1-based):
  //   1 -> *_one, 2 -> *_two, 3 -> *_three
  team_blue = blue_one, blue_two, blue_three
  team_red  = red_one,  red_two,  red_three

  // Waypoint tuning, applied to every dispatched click-waypoint
  capture_radius = 10.0
  slip_radius    = 15.0
  speed          = 2.0

  // Radius of the magenta control halo (meters)
  control_circle_radius = 10.0

  // Variable name overrides (defaults shown)
  // click_var        = RDR_CLICK
  // sel_team_var     = RDR_SEL_TEAM
  // sel_index_var    = RDR_SEL_INDEX
  // mode_toggle_var  = RDR_CLICK_MODE_TOGGLE
  // mode_state_var   = RDR_CLICK_MODE
  // untag_toggle_var = RDR_UNTAG_TOGGLE
  // untag_var        = REDIR_ALLOW_UNTAG
  // arrived_var      = REDIR_ARRIVED
  // controlled_var   = REDIR_CONTROLLED
  // active_var       = REDIR_ACTIVE
  // update_var       = REDIR_UPDATE

  debug = true
}
```

## Mission Integration

Full step-by-step edits (with rationale) live in `config.txt`. Summary:

1. **`meta_surveyor.bhv` — initializes.** `REDIR_CONTROLLED`, `REDIR_ACTIVE`, `REDIR_ALLOW_UNTAG`, and `REDIR_ARRIVED` all initialize to `false`.
2. **`meta_surveyor.bhv` — the three redirect behaviors** (pasted into the shared `<bhvs>` block):
   - `station_hold` (`BHV_StationKeep`, pwt 140, `center_activate=true`) — holds the boat where it is while controlled with no active click-waypoint. Because it has no fixed station point, re-activation after a redirect captures the arrival position, giving "hold at the clicked point" for free.
   - `waypt_redirect` (`BHV_Waypoint`, pwt 150, perpetual, `updates=REDIR_UPDATE`) — drives to the clicked point, outbidding station_hold. Endflags clear `REDIR_ACTIVE` and set `REDIR_ARRIVED`.
   - `constant_heading_redirect` (`BHV_ConstantHeading`, pwt 100, perpetual, `updates=REDIR_HEADING`) — **experimental, not fully tested.** Intended to point the boat on a commanded heading after arrival, gated on `REDIR_ARRIVED`. Nothing publishes `REDIR_HEADING` yet, so on arrival it activates with the behavior's default heading. Safe to comment out until finished; see `config.txt` §8.
3. **`meta_surveyor.bhv` — game-behavior gating.** Each game behavior (`waypt_grab_easy`, `waypt_grab_medium`, `loiter_passive`, `aggressive_station_keep`, `rlagent_attacker`) gets `condition = REDIR_CONTROLLED != true`. `waypt_untag` is gated differently — `condition = (REDIR_CONTROLLED != true) or (REDIR_ALLOW_UNTAG = true)` — so a controlled boat only auto-recovers home if the operator enabled it for that boat. Reactive collision avoidance, if enabled, stays ungated.
4. **`meta_shoreside.moos` / ANTLER** — `Run = pRedirectWaypoint @ NewConsole = false`
5. **`meta_shoreside.moos` / uFldShoreBroker** — qbridge the command vars:
   ```
   qbridge = REDIR_CONTROLLED
   qbridge = REDIR_ACTIVE
   qbridge = REDIR_UPDATE
   qbridge = REDIR_ALLOW_UNTAG

   // Only needed for the experimental arrival-heading feature:
   qbridge = REDIR_ARRIVED
   qbridge = REDIR_HEADING
   ```
6. **`meta_shoreside.moos` / pMarineViewer** — post clicks and add the toggle buttons:
   ```
   left_context = RDR_CLICK = x=$(XPOS),y=$(YPOS)
   button_13    = WCtrlTeam  # RDR_CLICK_MODE_TOGGLE=$[UTC]
   button_14    = UntagAllow # RDR_UNTAG_TOGGLE=$[UTC]
   scope        = RDR_CLICK_MODE
   ```
   Plain left-clicks are free in the stock config (SHIFT/ALT-click is the built-in drop-point gesture), so `RDR_CLICK` via `left_context` does not collide. The `b`/`r`/`1..9`/`w`/`u` selection keys come from the patched pMarineViewer.

## Building

Standard MOOS-IvP app build — drop this directory into your tree's `src/` and rebuild:

```bash
./build.sh        # or: cd build && cmake .. && make pRedirectWaypoint
```

Links against `MOOS`, `contacts`, `geometry`, `apputil`, `mbutil`, `m`, and `pthread` (see `CMakeLists.txt`).

## Command Line

```
Usage: pRedirectWaypoint file.moos [OPTIONS]
  --alias=<ProcessName>   Launch with the given process name
  --example, -e           Show example MOOS configuration block
  --help, -h              Show help
  --interface, -i         Show MOOS publications and subscriptions
  --version, -v           Show release version
```

## Repo Contents

| File | Purpose |
|---|---|
| `RedirectWaypoint.h/.cpp` | The AppCasting MOOS app: selection state, control set, click dispatch, control circles, appcast report |
| `RedirectWaypoint_Info.h/.cpp` | `--help` / `--example` / `--interface` text |
| `main.cpp` | Standard MOOS app entry point |
| `CMakeLists.txt` | Build target and link libraries |
| `config.txt` | Full mission-integration change list for `meta_surveyor.bhv` / `meta_shoreside.moos` |

---

**Author:** Tyler Errico — West Point Robotics Research Center, 2026
