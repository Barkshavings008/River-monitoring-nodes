# River Monitoring Nodes

Low-cost ESP32 stations that watch a river's water quality, work out what
kind of pollution is coming down it, and send alerts to a phone over
Bluetooth.

Each node measures **pH, TDS (dissolved salts), turbidity (cloudiness) and
water temperature**. It learns what "normal" looks like at its spot on the
river, then compares every minute against that to spot patterns such as
sewage, acid mine drainage, industrial discharge, construction runoff,
fertiliser, warm water or salt, while ignoring rain storms and faulty
sensors. Nodes along the river compare notes to narrow down where the
pollution is coming from and how fast the water is moving it downstream.

> Labels are likely candidates, not proof. Confirm anything serious with a
> lab water sample.

## What it does

- **Reads 4 sensors** every 2 s and takes the median each minute, so one bad
  reading can't set off an alert
- **Learns normal** for each sensor over 24 hours (and keeps pollution out of it)
- **Checks for broken sensors**: unplugged, out of range, stuck, or jumping
- **Names the pollution**: 9 pattern rules plus a rain filter, each with a
  confidence score and a list of likely pollutants
- **Doesn't flicker**: a label has to be seen 3 minutes in a row to turn on
  and be gone for 5 to turn off
- **Compares nodes** on branching rivers (streams joining a main river) to say
  whether pollution is coming from upstream, from one bank, or from between
  two nodes
- **Estimates water speed** from the same pollution reaching one node and then
  another further down
- **Sends phone alerts** over Bluetooth Low Energy to a web page that shows
  the readings, the pollution type, the water speed and every node

## How it works

```
 sensors ──> voltages ──> pH / TDS / NTU / °C ──> 1-minute medians
                                                       │
                                     sensor fault checks
                                                       │
                     compare with learned "normal" ──> pattern rules ──> persistence
                                                                              │
          serial monitor  <──  report  ──>  phone (Bluetooth)  <──  compare with other nodes
```

The full explanation, file by file, is in the
[code guide (PDF)](docs/River_Node_Code_Guide.pdf).

## Hardware

| Part | Notes | ESP32 pin |
|---|---|---|
| ESP32 DevKit (`esp32dev`) | The board everything runs on | |
| pH module (Logo-Rnaenaor V2.0) + probe | 5 V output, through a 12k / 10k voltage divider | GPIO 34 |
| TDS meter (STJF V1.0) | Outputs ≤ 2.3 V, wired straight in | GPIO 33 |
| Turbidity sensor (TS-300B type) | 0–4.5 V output, through a 12k / 10k voltage divider | GPIO 32 |
| DS18B20 waterproof temperature probe | Needs a 4.7k resistor from data to 3.3 V | GPIO 4 |

Divider wiring: sensor output → 12k → ESP32 pin → 10k → GND.

## Getting started

1. Install [VS Code](https://code.visualstudio.com/) and the
   [PlatformIO](https://platformio.org/install/ide?install=vscode) extension.
2. Open this folder (the one with `platformio.ini`) in VS Code.
3. In `src/config.h`:
   - `SIMULATE 1` runs on made-up readings, with no sensors needed. Set it to
     `0` for the real sensors.
   - `DEMO_MODE 1` shortens the learning times for quick testing. Set it to
     `0` for real use.
   - Calibrate `PH_V7`, `PH_V4` and `TURB_V_CLEAR` with the `cal` command
     (see below).
4. Plug in the ESP32 and click **→ Upload** in the PlatformIO bar. The
   libraries download by themselves.
5. Click the **plug icon (Serial Monitor)** to see the readings every minute.

### Serial monitor commands

| Command | What it does |
|---|---|
| `list` | Shows every node on the river |
| `add <id> <metres> [place]` | Adds a node |
| `del <id>` | Removes a node |
| `move <id> <metres>` | Moves a node |
| `cal` | Prints the raw sensor voltages, for calibrating |
| `help` | Lists the commands |

### Calibrating

- **pH**: put the probe in pH 7 buffer, type `cal`, and copy the pH voltage
  into `PH_V7`. Rinse it, then do the same in pH 4 buffer for `PH_V4`.
- **Turbidity**: put the sensor in clear water, type `cal`, and copy the
  voltage into `TURB_V_CLEAR`.
- Store the pH probe with its cap on and potassium chloride (KCl) storage
  solution inside. Never store it dry or in distilled water.

## Phone alerts

The board shows up over Bluetooth as `RiverNode-N1`. Open the phone page:

**https://barkshavings008.github.io/River-monitoring-nodes/phone/**

- **iPhone:** use the free **Bluefy** browser app (Safari can't use Bluetooth).
- **Android:** use Chrome.

Tap **Connect**, or **Try a demo** to see it without the board. Keep the page
open with the screen on to get alerts. Bluetooth reaches about 10–30 m.

The page is served by GitHub Pages from the `docs/` folder (Settings → Pages
→ Deploy from a branch → `main` / `docs`).

## Simulator

`simulator/index.html` is an interactive map with two streams joining a
river, six nodes and three factories whose waste you choose. It runs the real
node code from `src/`, compiled to WebAssembly. Download the file and open it
in a browser. See [simulator/README.md](simulator/README.md).

## Tests

```
pio test -e native
```

60 unit tests run on a computer: the pollution rules, sensor conversions,
fault checks, learning normal, persistence, the whole engine, the river
network, the water speed and the phone messages.

## Folders

| Folder | What's in it |
|---|---|
| `src/` | The node code for the ESP32 |
| `test/` | The unit tests |
| `docs/` | The code guide PDF and the phone page |
| `simulator/` | The interactive web simulator |
| `manual/` | A simpler, separate project with only the sensor and linked-list helpers, for writing the logic yourself (see [manual/README.md](manual/README.md)) |

## Limits

- Four sensors can't tell every pollutant apart, so labels are likely
  candidates only.
- The nodes don't talk to each other by radio yet. The other nodes are
  simulated (`SIM_REMOTE_NODES` in `config.h`).
- The water speed is only known after a pollution plume has passed two nodes.
- Phone alerts only arrive while the page is open and the phone is in
  Bluetooth range.
