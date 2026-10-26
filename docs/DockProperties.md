# Dock properties

Property numbers (`siid.piid`) for the MOVA P10 Ultra, checked against the MOVAhome robot plugin `dreame.vacuum.common` build 1239. The plugin is a React Native bundle that the app downloads per model, from `GET /dreame-product/upgrades/appplugin?model={MODEL}&os=1&appVer=10607`. Each service is defined in the bundle as `this.SIID = n` with `this.PIID[Props.X] = n`.

Polled by `DockMonitor` (meanings live in `src/Dreame/Types.h`):

| Property | Plugin name | Meaning |
|---|---|---|
| 2.1 | VacuumStatus | Robot state |
| 2.2 | VacuumFault | Error code (68 seen with no visible fault; meaning unknown) |
| 4.1 | WorkMode | Robot status |
| 4.7 | TaskStatus | Task status |
| 4.25 | WashStation | Base status |
| 4.41 | NoWaterWarn | Clean water tank warning. Stays raised until the app dismisses it (the app writes 1), so it can outlive a refill. |
| 27.1 | RisingBox | Clean water tank |
| 27.2 | DirtyWaterBox | Dirty water tank |
| 27.4 | Detergent | Cleaning fluid: 1 empty or not installed, 2 low |

## Water kit properties (not polled)

These belong to the official auto water supply/drain kit. ClankerCleaner keeps the normal removable tanks, so they're recorded here only.

| Property | Plugin name | Meaning | Value seen 2026-10-26 |
|---|---|---|---|
| 4.51 | AutoDrainWater | 1 = kit installed | 0 |
| 27.5 | WaterDrain | 1 = kit is emptying both tanks (station action `9,1`) | 0 |
| 4.60 | WaterTankDrainage | Robot water tank drain (station action `7,1`): 1 draining, 2 done, 3 failed; the app resets it to 0 | 0 |
| 4.54 | WaterSelfCheckStatus | Defined but never read by the plugin; the kit self-check shows as work mode 1501 instead | not reported |
| 27.9 | WaterBoxInstall | Polled by the app but never shown; probably robot water box or mop module fitted | 1 |

**4.51 is read-only as far as the app is concerned.** The plugin never writes it, so the dock most likely sets it from detecting the kit hardware. Writing 1 through the cloud is untested. If it were accepted, the app would show the kit self-check, the "empty tanks" button, a water tank filter consumable and the super wash level, and would hide the deodorizing module. The dock would probably try to fill and drain through a valve and pump that aren't there and fail partway through a mop wash. It also wouldn't clear tank warnings, which come from 27.1, 27.2 and 4.41.

27.5 and 4.60 could be added to the "base busy" check later: tank work shouldn't start while either is 1.

## Station actions

Action `4.4`, with the command in parameter 10 (`"code,value"`):

| Command | Does |
|---|---|
| `2,1` | Wash mop |
| `3,1` / `3,0` | Start / stop drying |
| `7,1` | Drain the robot's water tank |
| `9,1` | Kit: empty both tanks |
| `10,1` | Dust collection |

## Not decoded

- 4.35 (WarnStatus) reads 101.
- Error 68: the plugin's error text isn't stored under a searchable key.
- The plugin download's MD5 didn't match the one the server listed, so treat the bundle as unverified.
