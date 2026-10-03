# Proposed telemetry protocol v1

Implement and test in M6. This is a design contract, not an existing encoder. Wire integers are unsigned little-endian; floats are IEEE-754 binary32 little-endian. Serialize fields explicitly instead of writing native C struct memory with compiler-dependent padding.

| Offset | Bytes | Field |
|---:|---:|---|
| 0 | 2 | Magic: `A5 5A` |
| 2 | 1 | Version: 1 |
| 3 | 1 | Flags: bit 0 target valid, 1 range valid, 2 orientation valid; others zero |
| 4 | 2 | Payload length: 48 |
| 6 | 2 | Sequence, wraps modulo 65536 |
| 8 | 8 | Sample timestamp, microseconds since boot |
| 16 | 4 | Object X, pixels |
| 20 | 4 | Object Y, pixels |
| 24 | 4 | Distance, meters |
| 28 | 4 | Roll, radians |
| 32 | 4 | Pitch, radians |
| 36 | 4 | Yaw, radians, relative to initial heading |
| 40 | 4 | Servo command angle, radians relative to calibrated neutral |
| 44 | 4 | Confidence, 0–1 |
| 48 | 4 | Vision age, microseconds |
| 52 | 4 | Range age, microseconds |
| 56 | 2 | CRC-16/CCITT-FALSE over bytes 2–55 |

Total: 58 bytes. Payload is bytes 8–55. Timestamp is the orientation snapshot time; ages express how much earlier vision/range were sampled. Transmit only nonnegative ages; reject inconsistent timestamp assembly. For higher-motion accuracy, extend a later version with separate timestamps rather than concealing skew.

CRC parameters: polynomial `0x1021`, initial `0xFFFF`, no reflection, XOR-out `0x0000`. Check string `123456789` produces `0x29B1`. Store the resulting CRC little-endian. C and Python must agree on complete packet bytes, not merely this checksum check.

Python candidate format before checksum: `'<2sBBHHQ8fII'` (56 bytes). Assert its size with `struct.calcsize`. Invalid fields carry zeros with their validity bits cleared; consumers must check flags before using values. Reject nonfinite floats in fields flagged valid and implausible ranges/angles based on calibration.

At 20 packets/s this is 1160 bytes/s. For 8N1 UART, 115200 baud has a theoretical ceiling of 11520 bytes/s, before practical headroom. Select one baud rate on both ends. Do not mix printf/log strings into this binary stream; use a separate console. If sharing a UART during development, the resynchronizing parser must tolerate boot noise, but separate logging is the intended integrated design.

## Parser state machine

Append received chunks to a bounded buffer. Search for magic. Wait for the fixed header; reject unknown versions or incorrect length. Wait for 58 bytes; verify CRC, flags, and field validity. On failure discard one candidate byte and search again, not the entire buffer. On success consume the packet and emit one typed sample. Keep the final possible magic-prefix byte when discarding noise.

Set a maximum buffer length and incomplete-frame timeout. Handle disconnect, board reboot (timestamp resets), sequence wrapping, duplicate packets, and dropped packets. Do not join trajectories across reboot silently.

## Required tests

One-byte chunks; split at every possible boundary; multiple packets per read; garbage before magic; magic inside payload; bit flip; wrong version; oversized length; truncation then a valid frame; sequence wrap; reset; invalid flags; nonfinite floats. Keep one agreed C/Python golden packet and assert exact bytes. Random corrupted input must neither crash nor grow memory without bound.
