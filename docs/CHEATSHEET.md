# Modern embedded C cheatsheet

A one-page reminder of the decisions this course keeps asking you to make.
Each entry names the exercise that covers it.

## Choosing an integer type — 01.01, 01.02

| The value is... | Use |
|---|---|
| a register, wire field, or anything with a bit layout | `uint8_t` / `uint16_t` / `uint32_t` |
| a size or index | `size_t` |
| a pointer difference | `ptrdiff_t` |
| loop-local arithmetic with no layout meaning | `int` / `unsigned` |
| a tick count or timestamp | `uint32_t`, compared by subtraction (13.04) |

`int` is only guaranteed 16 bits. Mixed signed/unsigned comparisons convert
the signed side — `-1 > 0u` is true (02.02).

## Arithmetic that cannot betray you — 02.01–02.04, 02.09

- Operands smaller than `int` are **promoted to int** before any operator
  touches them: `~x`, `x << 24`, `a + b` on `uint8_t` all compute in signed
  32-bit. Cast back to the width you meant.
- Signed overflow is **UB**; unsigned wraps by definition. Check *before*
  the operation (`a > INT32_MAX - b`), or saturate — a pegged sensor beats a
  wrapped one.
- Multiply first, divide second (00.01); add half the divisor to round
  (15.05); widen to 64-bit before a Q16.16 multiply (02.09).
- Shifts: never by ≥ width or into a signed sign bit. `1u << n`, not `1 << n`.

## Bytes on a wire — 02.06, 05.02

Structs do not go on wires and byte buffers do not become structs by cast
(alignment + aliasing + byte order, 02.08). Pack and unpack explicitly:

```c
uint32_t v = ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16)
           | ((uint32_t)b[2] << 8) | (uint32_t)b[3];
```

`memcpy` is the blessed way to move unaligned bytes into a typed object.

## Where objects live — 06.04, 15.01, 15.02

| Object | Section | Costs |
|---|---|---|
| `static const` table | `.rodata` | flash only — prefer for lookup data |
| `static uint8_t buf[N];` | `.bss` | RAM; zeroed by startup |
| `static uint8_t buf[N] = {1};` | `.data` | RAM **and** flash (the init image) |
| big local array | stack | your 2 KB budget; move it to static (08.07) |

## Memory without malloc — chapter 08

| Need | Use |
|---|---|
| N identical objects, alloc/free in any order | object pool (08.04) |
| scratch space freed all at once | arena (08.05) |
| a byte pipe between contexts | ring buffer (08.06, 12.04) |
| variable-size messages | flexible array member in an arena (05.06) |

Check every capacity result; exhaustion is a status, not a surprise.

## Error handling — chapter 09

| The failure is... | Use |
|---|---|
| a bug in the caller | `assert` (dev builds; 09.05) |
| impossible by construction | `_Static_assert` (07.05) |
| a runtime condition (timeout, bad input, busy) | status enum, `0 == OK` |
| multiple resources to unwind | `goto cleanup`, reverse order (09.03) |

Leave out-parameters untouched on failure (09.02). Validate at trust
boundaries, assert between friends (09.06).

## Registers — chapter 11

- `volatile` = every access happens, in order, *relative to other volatiles*.
  Not atomicity, not a fence (11.01, 11.07).
- Multi-bit field: **clear, then set**. `|=` cannot write a 0 (11.03, 15.04).
- Status registers are often **write-1-to-clear**: `reg = FLAG` acknowledges
  one; `reg |= FLAG` acknowledges everything pending (11.04).
- Poll with a deadline, always (11.05). Enable a peripheral's clock, then
  read the enable back (15.04).

## Sharing with an ISR — chapter 12

| Shared thing | Use |
|---|---|
| one flag / one 32-bit word | `_Atomic uint32_t` (lock-free on Cortex-M4) |
| a 64-bit or multi-field value | critical section (PRIMASK save/restore, 12.03) |
| a byte/event stream | SPSC ring, release/acquire (12.04) |
| anything from an ISR | minimal work; defer to the main loop (12.07) |

ISR-callable code shares no mutable statics (12.06).

## Time — 13.04

Never compare absolute tick values. `(uint32_t)(now - start) >= duration`
survives wraparound; `now >= deadline` does not. A 1 kHz uint32_t tick wraps
in 49.7 days — inside your product's lifetime.

## Testing off-target — chapter 10

Hardware is a parameter. Inject the register pointer (10.01), the HAL struct
(10.02), the clock (10.04). Spy on outputs, stub inputs, mock only protocols
(10.03, 10.05). If logic can run on the host, it can meet AddressSanitizer,
UBSan and TSan (00.03) — none of which fit on the target.
