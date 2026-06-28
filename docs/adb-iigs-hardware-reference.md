# Apple IIgs Hardware Reference — ADB electrical timing

This document records **Apple Desktop Bus** timing and related rules from **Apple IIgs Hardware Reference** (Chapter 6, “The Apple Desktop Bus”), including **Table 6-8** (*ADB timing specifications*). It is the published IIgs-centric spec that explains **wider** attention and **bit-cell** bounds than the “nominal 100 µs / 800 µs attention” presentation in *Guide to the Macintosh Family Hardware*, 2nd edition.

**Source PDF:** `Apple IIgs Hardware Reference_HiRes.pdf` (local reference copy). That edition is **bitmap-only** (no text layer); values below were taken from the printed **Table 6-8** and surrounding sections via OCR and cross-checked against figure captions. Minor OCR artifacts (e.g. “Syne” for Sync) are normalized here.

For how this relates to firmware branches, see `docs/adb-iigs-support.md`.

---

## Bit encoding (duty cycle)

The IIgs book defines bits by **low-time as a fraction of bit-cell time**:

- Low period **&lt; 35%** of bit-cell time → bit **1**
- Low period **&gt; 65%** of bit-cell time → bit **0**

This matches the usual ADB mapping (short low = 1, long low = 0) when the cell is near 100 µs.

---

## Table 6-8 — ADB timing specifications

| Parameter | Minimum | Maximum | Unit / note |
|-----------|---------|---------|-------------|
| Bit-cell time | 70 | 130 | microseconds |
| “0” low time | 60% | 70% | of bit-cell time |
| “1” low time | 30% | 40% | of bit-cell time |
| Attention | 560 | 1040 | microseconds |
| Global Reset | 2.8 | 5.2 | milliseconds |
| Sync | 60% | 70% | of bit-cell time |
| Service request | 140 | 260 | microseconds |
| Stop bit to start bit time (Tlt) | 140 | 260 | microseconds |

**Notes:**

- **Attention 560–1040 µs** is the range that motivates accepting attention **outside** a tight ~800 µs window when implementing a **device** on the IIgs host.
- **Sync** is specified **relative to bit-cell** (60–70%), not only as a fixed microsecond value (contrast *Guide* Table 8-14, which gives **65 µs ±3%** for Sync).
- **Tlt** **140–260 µs** aligns with the *Guide* stop-to-start bounds.

---

## Global Reset (prose)

When the bus is held low for **at least 2.8 ms**, a **Global Reset** is initiated. Only the **host** may issue it; all devices reset. This is distinct from the **Device Reset** command, which targets one address.

---

## Service Request (prose + table)

A device may assert **SRQ** by holding the bus low during the **low portion of the stop bit** after a command. The stop must be lengthened by **at least 140 µs beyond the normal bit-cell boundary** (Figure 6-12). The **140–260 µs** row in Table 6-8 is the **documented range** for the service-request timing; the **≥ 140 µs** extension is the **minimum** called out in text.

---

## IIgs-specific policy

The **Apple IIgs** reference states that **ADB mouse devices are prohibited from issuing Service Requests** on the IIgs; other devices may, if allowed via register 3.

---

## Comparison with *Guide to the Macintosh Family Hardware* (2e, Table 8-14)

These two Apple documents are **consistent in spirit** but **not identical** in how they tabulate numbers:

| Topic | IIgs Hardware Reference (Ch. 6) | *Guide* (Table 8-14) |
|--------|--------------------------------|----------------------|
| Bit cell | **70–130 µs** min/max | **100 µs** nominal; host ±3%, device ±30% |
| Attention | **560–1040 µs** | **800 µs ±3%** (~776–824 µs) |
| Sync | **60–70%** of bit-cell | **65 µs ±3%** (absolute) |
| Tlt | **140–260 µs** | **200 µs** nominal; **140–260 µs** min/max |
| SRQ | **140–260 µs** in table; **≥140 µs** extension in prose | **300 µs ±30%** in table; **≥140 µs** extension in prose |
| Global reset | **2.8–5.2 ms** | **≥3 ms** minimum |

The **IIgs Hardware Reference** is the document that explicitly gives **560–1040 µs** attention and **70–130 µs** bit cells—useful for **device-side receive** validation on an IIgs host. The **Macintosh Family Hardware** table remains the common reference for **nominal** 100 µs cells and **800 µs** attention on many Mac implementations.

---

## Related project notes

- `docs/adb-iigs-support.md` — firmware differences (`feature/display` vs `feature/IIGS-Fixes`).
- `docs/iigs-debugging.md` — debugging ideas for fast typing / IIGS behavior.
