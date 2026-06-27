# ADC Sensor Log Processor

UFMFGT-15-1 Programming for Engineers (Resit) — a command-line C99 program
that reads a binary log from a 4-channel, 12-bit, 1 kHz ADC, analyses it, and
writes a structured results report.

**GitHub repository:** [https://github.com/Muaath2Almutairi/adc-sensor-log-processor](https://github.com/Muaath2Almutairi/adc-sensor-log-processor)

## What it does

1. Reads and validates the 24-byte file header (magic number, version).
2. Loads every 16-byte record into a heap-allocated `ADCSample` array.
3. Converts each raw 12-bit code to a voltage: `(raw / 4095.0) * 3.3`.
4. Computes per-channel mean, min, max and standard deviation.
5. Counts faults per channel (overvoltage > 3.0 V, undervoltage < 0.3 V,
   sensor-fault status bit).
6. Checks sequence-number integrity and reports any gaps.
7. Writes `results.txt`.

Extensions also implemented: voltage histogram (10 bins), per-channel
temperature analysis with a 60 °C threshold, a sliding-window rolling mean,
and a per-record `fault_report.txt`.

## Source layout

| File | Responsibility |
|------|----------------|
| `main.c` | Entry point only: argument parsing, orchestration, `free`, exit. |
| `adc.c` / `adc.h` | Analysis: voltage conversion, per-channel stats, fault detection, integrity, extensions. |
| `io.c` / `io.h` | All file I/O: binary parsing, header validation, writing reports. |
| `stats.c` / `stats.h` | Standalone numeric primitives: mean, min, max, std-dev. |
| `CMakeLists.txt` | C99 build configuration. |

## Build and run

### Command line (gcc)

```bash
gcc -std=c99 -Wall -Wextra -o adc_processor main.c adc.c io.c stats.c -lm
./adc_processor adc_sensor_log.bin
```

An optional second argument sets the sliding-window size (default 8):

```bash
./adc_processor adc_sensor_log.bin 16
```

### CLion / CMake

1. Open the project folder in CLion (it detects `CMakeLists.txt`).
2. Select the `adc_processor` build configuration.
3. **Build → Build Project** (or the hammer icon).
4. Edit the run configuration’s *Program arguments* to `adc_sensor_log.bin`,
   and set the *Working directory* to the folder that contains the dataset.
5. **Run**.

Equivalent manual CMake build:

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
cmake --build .
./adc_processor ../adc_sensor_log.bin
```

## Output files

- `results.txt` — header summary, per-channel voltage statistics, fault
  counts, temperature analysis, histogram, and sampling integrity.
- `fault_report.txt` — one line per faulted record with timestamp, channel,
  raw value, voltage, and fault type(s).

## Notes

The on-disk record is a **packed** 16-byte structure. The program reads it
into an explicitly packed struct (`__attribute__((packed))` / `#pragma pack`)
and asserts `sizeof == 16` at compile time, then copies fields into the
naturally-aligned in-memory `ADCSample`. See the logbook for the full
explanation of why this matters.

## Fault Reporting Details

Fault report logs individual records that exceed voltage bounds or trigger the sensor fault bit.


Tested with AddressSanitizer (ASan) to confirm zero memory leaks.

## Fault Reporting Details

Fault report logs individual records that exceed voltage bounds or trigger the sensor fault bit.

## Sequence Integrity

Detects gaps in sequence numbers and reports missing records.

## Extensions

Histogram, temperature analysis, sliding-window rolling mean, and per-record fault_report.txt.


Tested with AddressSanitizer (ASan) to confirm zero memory leaks.
