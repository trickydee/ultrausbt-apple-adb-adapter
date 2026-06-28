# ADB host mode — adbmon capture checklist

Work from this document when sniffing **real Mac host → ADB device** traffic with **adbmon**.  
Primary reference target today: **trackball + Quadra 700** (same OS you will use with the adapter in host mode).

**Related:** [`adb-host-mode.md`](adb-host-mode.md), [`gravis-mousestick-ii-plan.md`](gravis-mousestick-ii-plan.md) (Gravis / MouseStick later), [`FUTURE_WORK.md`](FUTURE_WORK.md) §5 (ADBMON).

Save captures under `docs/fixtures/adb-host/` with descriptive names.

---

## File header (paste at top of every log)

```text
Mac: Quadra 700
OS: System _._ (_exact version from About This Mac_)
Device: _trackball model / handler if known_
Capture ID: H1-boot  (see table below)
Date:
Notes:
```

---

## Command byte quick reference

**Log format (adbmon 0.2+):**

```text
ADB evt=Talk raw=3C addr=3 reg=0 srq=1 data=- t_us=52141041
ADB evt=Talk raw=FC addr=F reg=0 srq=0 data=2:8081 t_us=52152932
ADB evt=GlobalReset t_us=11566843
# adbmon checkpoint id=1 capture=ON t_us=...
```

| Field | Meaning |
|-------|---------|
| `evt` | Reset / Flush / Talk / Listen / GlobalReset |
| `raw` | Command byte (hex) |
| `addr` / `reg` | ADB address and register |
| `srq` | `1` = device stretched stop bit (service request) |
| `data=-` | No payload (Talk timeout or Flush/Reset) |
| `data=2:8081` | 16-bit payload (2 bytes, hex) |

**BOOTSEL:** short tap = checkpoint marker; hold ~0.8 s = capture ON/OFF (see [`src/adbmon/README.md`](../src/adbmon/README.md)).

Legacy `ADB Command:…` lines (adbmon 0.1) — same fields, less structured.

| adbmon 0.1 line | Meaning |
|-------------|---------|
| `cmd=3C` | **Talk** reg **0**, addr **3** |
| `cmd=3F` | **Talk** reg **3**, addr **3** (identity / handler) |
| `cmd=2C` | **Talk** reg **0**, addr **2** (keyboard) |
| `cmd=2F` | **Talk** reg **3**, addr **2** |
| `cmd=38`–`3B` | **Listen** reg 0–3 |
| `cmd=F0` | **Reset** (all devices) |
| `Data[2]:ABCD` | 16-bit payload (4 hex digits = 2 bytes, **big-endian**) |

**Addr** = `(cmd >> 4) & 0x0F`, **reg** = `cmd & 0x03`, **Talk** when `(cmd & 0x0C) == 0x0C`.

---

## Priority 1 — Host-mode essentials (do first)

### H1 — Cold boot (~15 s)

| | |
|-|-|
| **Do** | Mac **off** → trackball on ADB → power on. Log from first bus activity. |
| **Save as** | `q700-trackball-H1-boot.txt` |
| **Need** | Reset/Flush; **Talk R3** @ pointing device address; first **Talk R0** idle |

### H2 — Idle (30 s)

| | |
|-|-|
| **Do** | Desktop up, do not touch trackball. |
| **Save as** | `q700-trackball-H2-idle.txt` |
| **Need** | Poll cadence (~ms between `Talk`); stable idle **Data[2]**; any **SRQ** |

### H3 — Slow movement (four clips)

| | |
|-|-|
| **Do** | ~10 s each: move ball **left**, **right**, **up**, **down** (slow). |
| **Save as** | `q700-trackball-H3-left.txt` … `-right`, `-up`, `-down` |
| **Need** | How **Data[2]** changes per direction |

### H4 — Buttons

| | |
|-|-|
| **Do** | Left click only → right click only → both (if supported). |
| **Save as** | `q700-trackball-H4-buttons.txt` |
| **Need** | Button bits in reg0 (Apple: bit **15** left, bit **7** right; **0** = pressed) |

### H5 — Drag

| | |
|-|-|
| **Do** | Hold left button, move ball. |
| **Save as** | `q700-trackball-H5-drag.txt` |
| **Need** | Movement + button in same **Talk R0** response |

### H6 — Golden lines (paste into issues / chat)

Pick **one** idle and **one** moving line in this exact form:

```text
ADB Command:__ (Talk) addr:_ reg:0 Data[2]:____ @______ us
```

Plus one **Talk R3** line from **H1** if present:

```text
ADB Command:__ (Talk) addr:_ reg:3 Data[2]:____
```

---

## Priority 2 — Multi-device (when keyboard is on bus)

| ID | Session | Save as pattern |
|----|---------|-----------------|
| H8 | Keyboard + trackball, idle | `q700-kbd+ball-H8-idle.txt` |
| H9 | Move trackball only | `q700-kbd+ball-H9-move.txt` |

**Need:** Does Mac alternate **Talk @2** and **Talk @3**? SRQ when ball moves?

---

## Priority 3 — Gravis MouseStick II (separate hardware)

When you have a real MouseStick II + Gravis cdev, use [`gravis-mousestick-ii-plan.md`](gravis-mousestick-ii-plan.md) §3 (sessions V1–V18).  
Trackball captures **do not** replace Gravis **handler 0x23** / 7-byte Talk R0.

---

## What to send back (minimum for firmware work)

1. **H1** — first 30–50 lines after power-on  
2. **H6** — one idle + one moving **Talk R0** line (with **Data[2]**)  
3. One **Talk R3** line from boot (address + handler)  
4. **System version** on the Quadra  

---

## Regressions / fixtures

After review, checked-in “golden” snippets live in:

```text
docs/fixtures/adb-host/
```

Filename: `q700-trackball-<session>-golden.txt`

---

## Checklist (tick when done)

- [ ] H1 Cold boot
- [ ] H2 Idle 30 s
- [ ] H3 Left / right / up / down
- [ ] H4 Buttons
- [ ] H5 Drag
- [ ] H6 Golden lines copied
- [ ] H8–H9 (optional, with keyboard)
- [ ] OS version recorded
