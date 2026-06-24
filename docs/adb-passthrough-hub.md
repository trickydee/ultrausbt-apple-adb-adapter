# ADB passthrough hub

Dual-port HIDHopper-style hardware connects **two ADB sockets to one open-collector bus**. Downstream devices (trackball, joystick, second keyboard) see the same traffic as the host.

## Problem

The adapter and a chained **trackball** both default to **mouse address 0x03**. The host’s Talk commands collide — two devices answer on the same address.

ADB allows **multiple pointing devices**; each must have a **unique address** (0x1–0x7). The host resolves conflicts during **enumeration** (Talk/Listen register 3, collision detection).

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

## Related

- [`adb_device_list.md`](adb_device_list.md) — handler IDs at address 0x3
- [`gravis_mousestick_ii.md`](gravis_mousestick_ii.md) — Gravis stick protocol
- [`HIDHopper.md`](HIDHopper.md) — dual ADB ports
- [`FUTURE_WORK.md`](FUTURE_WORK.md) §7
