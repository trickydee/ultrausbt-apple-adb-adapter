# ADB passthrough hub

Dual-port HIDHopper-style hardware connects **two ADB sockets to one open-collector bus**. Downstream devices (trackball, joystick, second keyboard) see the same traffic as the host.

## Problem

### Address collision at 0x03

The adapter and a chained **trackball** both default to **mouse address 0x03**. The host’s Talk commands collide — two devices answer on the same address.

ADB allows **multiple pointing devices**; each must have a **unique address** (0x1–0x7). The host resolves conflicts during **enumeration** (Talk/Listen register 3, collision detection).

### Handoff: trackball idle → USB/BT mouse (SRQ)

After Phase 1 relocation, the trackball may keep **0x03** and the adapter moves to **0x04–0x07**. A second problem appears at runtime:

- ADB is **host-driven**: devices only send movement when the host issues **Talk register 0** to their address.
- **SRQ (Service Request)** is how a device asks to be polled — it **stretches the stop bit** after a command (~140–260 µs extra low on the DATA line). It is not “speak”; **Talk** is the host read, **SRQ** is the device saying “please poll me.”
- While the trackball moves, the host polls **0x03** often and USB/BT movement can work (often because the bus stays active).
- When the trackball goes **idle**, the host may **stop polling** the adapter’s address if nothing asserts SRQ. Emulated mouse data sits in `mousepending` until a **keyboard SRQ** wakes the bus (e.g. a keypress, or right-click in ctrl-click mode which enqueues Ctrl).
- **Mouse SRQ** on the adapter fixes wake-up but **disrupts smooth movement** — periodic SRQ during drag causes visible hitches/pauses. Experiments with continuous and rate-limited hub mouse SRQ were reverted (see [`FUTURE_WORK.md`](FUTURE_WORK.md) §9).
- Default firmware keeps **`ADB_IIGS_MOUSE_SUPPRESS_SRQ=ON`** (keyboard SRQ only). The **Apple IIgs Hardware Reference** states that **ADB mouse devices must not issue SRQ** on the IIgs; other device types may if enabled in register 3.

**Core tension:** smooth USB/BT movement wants **no mouse SRQ during active polling**; handoff after trackball idle wants **something** to make the host poll the adapter’s address again.

**Verify enumeration:** OLED footer should show distinct addresses (e.g. `Hub K2 M4` with trackball on **0x03**, adapter on **0x04+**). If both appear at **M3**, relocation did not complete — software mitigations are much harder.

## Phase 1 (implemented)

**Host-driven address assignment** per the ADB spec — not hard-coded adapter addresses.

| Step | Behaviour |
|------|-----------|
| Power / global reset | Keyboard **0x02**, mouse **0x03** (Apple defaults) |
| Host Talk register 3 | Adapter proposes a relocation address (random per “Space Aliens”; current address after assignment) |
| Bus collision | GPIO collision detect; loser skips the next Listen **0xFE** |
| Host Listen register 3 / **0xFE** | Adapter adopts the host-assigned address |
| Hub mode + mouse R3 collision | Next proposal biased to **0x04–0x07** so **0x03** can stay with a chained trackball/joystick |

### Firmware

- `src/adb_hub.c` / `include/adb_hub.h` — hub flag, enumeration state, register-3 proposals
- Flash: `reserved_bytes[0]` = hub mode on/off (`[1]`/`[2]` reserved)
- `AdbInterface::Reset()` restores **0x02/0x03** and clears host-assignment state
- **Devices** OLED footer: `Hub K2 M3` (live addresses after enumeration, e.g. `Hub K2 M6`)

### Build option

```bash
# Default: hub mode ON for new / upgraded flash
cmake ... -DADB_PASSTHROUGH_HUB_DEFAULT=ON

# Single-port / no daisy-chain bias
cmake ... -DADB_PASSTHROUGH_HUB_DEFAULT=OFF
```

### Electrical pass-through

With both ports wired to the same DATA line, host traffic reaches downstream devices automatically. The adapter only **drives** the bus when responding as `kbd_addr` or `mouse_addr`. When not addressed, `ADB_OUT` stays released (high) so other devices can reply.

SRQ from downstream devices is visible on the shared line when the adapter is not pulling low.

## Phase 2 (future)

**Active repeater** for split host/device bus segments (separate GPIO pairs + PIO bit forwarding). Needed only if hardware isolates the two ports instead of paralleling them. Tracked in [`FUTURE_WORK.md`](FUTURE_WORK.md).

## Host behaviour

After a **global ADB reset** or Mac power-on, the host re-enumerates from default addresses. Chained physical devices (trackball, Gravis stick, ADB keyboard) participate in the same collision/relocation dance.

Power-cycle the Mac after firmware changes so enumeration runs cleanly.

## Options for smooth movement + chained devices

No single fix is implemented yet beyond Phase 1 relocation and suppressed mouse SRQ. Candidate approaches (see also [`FUTURE_WORK.md`](FUTURE_WORK.md) §9):

| # | Approach | Idea | Pros | Cons |
|---|----------|------|------|------|
| 1 | **Address separation only** | Reliable relocation; host polls every live pointing-device address | Spec-correct; no SRQ; silky movement | Host may favour last-active pointer; must verify Talks adapter addr |
| 2 | **Bus snooping** | Watch DATA; respond only on Talk @ adapter address; never collide with trackball | No SRQ; respects open-collector rules | Does not wake host if it stops polling adapter addr |
| 3 | **One-shot conditional SRQ** | SRQ only when pending + host has not Talk’d our `mouse_addr` recently (+ optional downstream idle) | Better than SRQ flooding | Still may hitch on pulse; IIgs discourages mouse SRQ |
| 4 | **Snoop downstream SRQ** | Detect trackball SRQ while moving; one adapter SRQ only when downstream idle + we have pending | SRQ only in handoff window | Needs stop-bit snoop on shared DATA |
| 5 | **Separate ADB addresses (§8)** | USB/BT at dedicated addr (e.g. 0x04), trackball at 0x03 — two bus mice | Native multi-pointer model | Mac multi-cursor support varies; more firmware |
| 6 | **Register 3 SRQ-enable bit** | Advertise SRQ capability only when hub + downstream present | May influence host scheduling | Unclear on classic Mac; IIgs mouse SRQ prohibited |
| 7 | **Refresh `mousereg0` while pending** | Update snapshot while `mousepending` so delayed Talk is not stale | Smoother after wake | Does not make host poll |
| 8 | **Workflow / user** | Keypress to wake; power-cycle after topology change | Works today | Not automatic |

**Practical direction:** verify relocation (1), add bus snoop (2), wake only on handoff with one-shot logic (3/4), consider multi-address pointing devices (5). **Avoid** mouse SRQ during continuous movement.

**Platform note:** `ADB_IIGS_MOUSE_SUPPRESS_SRQ=ON` (default) is correct for IIgs. Hub users on Mac may use `-DADB_IIGS_MOUSE_SUPPRESS_SRQ=OFF` for legacy mouse SRQ, but that trades smoothness for wake-up — not recommended for hub + trackball without smarter detection.

## Related

- [`adb_device_list.md`](adb_device_list.md) — handler IDs at address 0x3
- [`gravis_mousestick_ii.md`](gravis_mousestick_ii.md) — Gravis stick protocol
- [`HIDHopper.md`](HIDHopper.md) — dual ADB ports
- [`FUTURE_WORK.md`](FUTURE_WORK.md) §7 (hub Phase 2), §8 (multiple pointing devices), §9 (intelligent mouse SRQ)
- [`iigs-debugging.md`](iigs-debugging.md) §10 — mouse SRQ suppression
- [`adb-iigs-hardware-reference.md`](adb-iigs-hardware-reference.md) — SRQ timing; IIgs mouse SRQ policy
