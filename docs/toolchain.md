# Toolchain rulings

## ESP-IDF for on-chip tests (ruled 2026-10-03)

Asked by soroban through workspace-basecamp (charter: any third-party framework needs Michael's
ruling). Michael's answer, as basecamp relayed it (chillacks #3724): "yes, that's totally
acceptable, and fine to do on future boxes we commission."

The shape that answer approved, a standing rule for any box we commission:

- Espressif's official image, **`espressif/idf:v5.4.2`**, pinned to that tag (the tag lives in
  `tests/device/run.sh`; change it only by a commit that says why).
- Flashing and capture through `docker --device` for the board's serial port only, with the
  port's group passed in; nothing installed on the host; the agent user stays out of `dialout`.
- No board-vendor BSP code copied; the test app touches only the serial console.
- ESP-IDF is Apache-2.0. It builds the test app in `tests/device/`; the core in `core/` stays
  portable C11 with no platform calls.
