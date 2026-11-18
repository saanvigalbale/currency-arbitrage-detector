# Currency Arbitrage Detector

A command-line tool that detects profitable currency arbitrage opportunities in a set of foreign exchange rates. It models currencies as vertices and exchange rates as directed edges in a weighted graph, then applies the Bellman-Ford algorithm to identify negative-weight cycles — which correspond to arbitrage opportunities.

---

## Table of Contents

- [Overview](#overview)
- [How Arbitrage Detection Works](#how-arbitrage-detection-works)
- [Features](#features)
- [Technology Requirements](#technology-requirements)
- [Building the Project](#building-the-project)
- [Running the Application](#running-the-application)
- [CLI Reference](#cli-reference)
- [CSV Input Format](#csv-input-format)
- [Sample Datasets](#sample-datasets)
- [Running the Test Suite](#running-the-test-suite)
- [Limitations and Disclaimer](#limitations-and-disclaimer)

---

## Overview

Currency arbitrage occurs when a sequence of currency conversions returns more than the original amount.  For example, if converting USD → EUR → GBP → USD produces more USD than you started with, an arbitrage opportunity exists.

This tool reads exchange rates from a CSV file, builds a directed graph, and determines whether any such profitable cycle exists using a provably correct graph algorithm.

---

## How Arbitrage Detection Works

1. **Graph construction** — Each currency becomes a vertex. Each exchange rate `r` from currency A to currency B becomes a directed edge with weight `−log(r)`.

2. **Bellman-Ford relaxation** — All vertex distances are initialised to `0.0` (not infinity), which ensures that negative cycles in any disconnected component of the graph can still be detected. The algorithm runs for V iterations (where V is the number of currencies).

3. **Negative-cycle detection** — Any edge still relaxed on the final (V-th) iteration lies on a negative-weight cycle, which corresponds to `∏ rates > 1.0` — a profitable arbitrage.

4. **Cycle reconstruction** — The algorithm follows predecessor edges V times to land inside the cycle, then extracts and reverses the directed path to produce a human-readable conversion sequence.

5. **Profit calculation** — The compound multiplier is computed as the product of the original exchange rates around the cycle. The profit percentage is `(multiplier − 1.0) × 100`.

A minimum profit threshold of `1e-6` above 1.0 is applied to suppress numerical noise from floating-point arithmetic.

---

## Features

- Bellman-Ford negative-cycle detection on `−log(rate)` edge weights
- Handles disconnected currency components (all distances initialised to 0)
- Correctly selects the optimal edge when parallel edges exist between the same two currencies
- Self-loop arbitrage detection
- Strict CSV validation with per-line error reporting
- Structured diagnostic output when CSV loading fails
- Starting capital simulation with compound return calculation
- Sample exchange-rate datasets included
- Comprehensive automated test suite (5 CTest targets, 40+ test cases)

---

## Technology Requirements

| Requirement | Version |
|---|---|
| C++ Standard | C++17 |
| Compiler | GCC 15.x (MSYS2 / MinGW-w64) |
| Build System | CMake 3.20+ |
| Build Tool | Ninja (recommended) |
| Operating System | Windows (also works on Linux/macOS) |

**Installing MSYS2 / MinGW-w64 (Windows)**

1. Download and install [MSYS2](https://www.msys2.org/).
2. Open the MSYS2 MinGW64 shell and run:
   ```sh
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
   ```
3. Add `C:\msys64\mingw64\bin` to your Windows `PATH`.

---

## Building the Project

```powershell
# Clone or enter the project directory
cd currency-arbitrage-detector

# Configure (first time only)
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Build all targets
cmake --build build
```

The build produces:
- `build/currency_arbitrage.exe` — the main application
- `build/test_graph.exe`, `build/test_arbitrage.exe`, `build/test_rate_loader.exe`, `build/test_end_to_end.exe`, `build/test_cli.exe` — test executables

---

## Running the Application

```powershell
# Show concise usage
.\build\currency_arbitrage.exe

# Full help
.\build\currency_arbitrage.exe --help

# Run on a CSV file (uses default starting capital of 1000)
.\build\currency_arbitrage.exe data\sample_rates.csv

# Run with a custom starting capital
.\build\currency_arbitrage.exe data\sample_rates.csv 50000

# List the included sample datasets
.\build\currency_arbitrage.exe --list-samples
```

### Example output — arbitrage found

```
============================================================
                Currency Arbitrage Detector
  Graph-Based Foreign Exchange Arbitrage Detection System
============================================================

Loaded 5 exchange rates across 4 currencies from: data/sample_rates.csv

------------------------------------------------------------
                Arbitrage Opportunity Found!
------------------------------------------------------------

Conversion Cycle:
  USD -> JPY -> EUR -> USD

Exchange Details:
  Step 1: 1 USD -> 150.5 JPY
  Step 2: 1 JPY -> 0.0061 EUR
  Step 3: 1 EUR -> 1.0870 USD

Yield Metrics:
  Compound Multiplier: 1.000967
  Estimated Profit: 0.0967%

Simulated Returns:
  Starting Capital: 1000.00 USD
  Final Amount: 1000.97 USD
  Estimated Profit Amount: +0.97 USD
------------------------------------------------------------
```

### Example output — no arbitrage

```
------------------------------------------------------------
                 No Arbitrage Detected
------------------------------------------------------------
No arbitrage opportunity detected in the supplied exchange rates.
------------------------------------------------------------
```

### Example output — CSV error

```
------------------------------------------------------------
                  CSV Load Error
------------------------------------------------------------
  File:   data/bad_rates.csv
  Line:   3
  Reason: CSV Error (line 3): Rate must be strictly positive: '-0.85'

  Suggestion: Verify the CSV contains rows in the format:
              source_currency,target_currency,rate
              Rates must be finite, positive numbers.
              Run with -h/--help for full format details.
------------------------------------------------------------
```

---

## CLI Reference

| Command | Description |
|---|---|
| `currency_arbitrage <path.csv>` | Detect arbitrage using the given CSV; default capital 1000 |
| `currency_arbitrage <path.csv> <capital>` | Same, with a custom positive starting capital |
| `currency_arbitrage --list-samples` | List bundled sample datasets |
| `currency_arbitrage -h` / `--help` | Show full help message |

**Exit codes:** `0` on success (arbitrage found or not), `1` on any error (bad CSV, invalid arguments, etc.).

---

## CSV Input Format

The CSV file must contain rows with exactly three comma-separated fields:

```
source_currency,target_currency,rate
```

| Field | Description |
|---|---|
| `source_currency` | Currency code you are converting **from** (e.g. `USD`) |
| `target_currency` | Currency code you are converting **to** (e.g. `EUR`) |
| `rate` | Exchange rate: 1 unit of source = rate units of target |

**Rules:**
- A header row is detected automatically and skipped.
- Blank lines and whitespace around fields are ignored.
- Rates must be finite, strictly positive numbers (no zero, negative, NaN, or infinity).
- Currency codes may contain letters, digits, `_`, `-`, and `.`.
- Multiple directed edges between the same pair of currencies are allowed.

**Minimal example:**

```csv
source_currency,target_currency,rate
USD,EUR,0.92
EUR,GBP,0.85
GBP,USD,1.35
```

---

## Sample Datasets

Two sample files are included in the `data/` directory. Run `--list-samples` to verify their paths.

| File | Contents |
|---|---|
| `sample_rates.csv` | 5 exchange rates across USD, EUR, GBP, JPY — contains an arbitrage opportunity |
| `equilibrium_rates.csv` | 6 exchange rates across USD, EUR, GBP — no arbitrage opportunity |

```powershell
.\build\currency_arbitrage.exe data\sample_rates.csv
.\build\currency_arbitrage.exe data\equilibrium_rates.csv
```

---

## Running the Test Suite

```powershell
# Build first, then run all tests
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected output:
```
100% tests passed, 0 tests failed out of 5

Total Test time (real) = X.XX sec
```

| Test Target | Coverage |
|---|---|
| `TestCurrencyGraph` | Graph construction, rate validation, adjacency lists |
| `TestArbitrageDetector` | Bellman-Ford, 10 scenarios: disconnected components, parallel edges, self-loops, extreme rates, multi-candidate recovery |
| `TestRateLoader` | CSV parsing, 12 scenarios: headers, whitespace, blank lines, invalid fields, NaN/inf, empty files, parallel edges |
| `TestEndToEnd` | Reporter formatting, capital simulation, help/usage strings, CSV error diagnostics |
| `TestCliExecutable` | Actual binary invocation via subprocess, 8 CLI scenarios including invalid CSV |

---

## Limitations and Disclaimer

> **This tool is for educational and analytical purposes only. It does not constitute financial advice.**

**Transaction costs** — Real currency exchanges involve broker fees, bank spreads, and commissions. A theoretical 0.1% arbitrage profit would typically be wiped out by transaction costs before a single conversion completes.

**Bid/ask spread** — Exchange rates quoted for buying and selling a currency differ. The tool uses a single rate per directed edge. Real arbitrage calculations must account for both sides of the spread.

**Slippage** — Large trades move the market price. A rate available for a small test trade may not hold for the full capital amount.

**Liquidity** — Some currency pairs have limited trading volume. Converting a large amount may be impossible at the quoted rate.

**Execution timing** — Exchange rates change continuously. A detected arbitrage opportunity may disappear in the milliseconds between detection and execution.

**Single-cycle output** — The detector returns the first profitable cycle found. It does not enumerate all possible arbitrage cycles or rank them by profitability.

**Network and execution risk** — Any automated trading system must handle API failures, network latency, partial fills, and exchange outages.
