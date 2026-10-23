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
- **Sends phone notifications** over WiFi (through the free ntfy app) with
  the pollution type, the readings, where it's coming from and the water speed,
  plus a Bluetooth phone page as a backup

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
   - `DEMO_MODE 1` makes every "minute" 10 seconds long, so learning normal
     and alerts happen quickly. Set it to `0` for real use.
   - `DEMO_ALERTS 1` turns every yellow "watch" label (salt, fertiliser,
     warm water, etc.) into a red alert, so it sends a notification and trips
     the alarm servo. Set it to `0` for real use.
   - Calibrate `PH_V7`, `PH_V4` and `TURB_V_CLEAR` with the `cal` command
     (see below).
   - Set the turbidity noise limits `BASE_FLOOR_NTU` and `NTU_MIN_RISE`
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
| `arm` / `disarm` | Turns sensor-problem notifications on or off (remembered after turning off) |
| `test` | Sends a test notification to your phone |
| `wifi` | Shows whether WiFi is connected and how many notifications were sent |
| `help` | Lists the commands |

### Calibrating

- **pH**: put the probe in pH 7 buffer, type `cal`, and copy the pH voltage
  into `PH_V7`. Rinse it, then do the same in pH 4 buffer for `PH_V4`.
- **Turbidity**: put the sensor in clear water, type `cal`, and copy the
  voltage into `TURB_V_CLEAR`. Then leave it in still tap water for 30
  minutes and watch how much the turbidity reading wobbles from minute to
  minute. Set `BASE_FLOOR_NTU` and `NTU_MIN_RISE` a little above that
  (they start at 20 and 25 NTU).
- Store the pH probe with its cap on and potassium chloride (KCl) storage
  solution inside. Never store it dry or in distilled water.

## Phone alerts

### WiFi notifications (main)

Pop-ups on your phone through **ntfy**, a free app for iPhone and Android:

- **Pollution / watch alert:** what it probably is and how sure, the 4
  readings next to normal, likely pollutants, where the source probably is,
  and the water speed
- **Water speed:** when the same pollution reaches a second node
- **Sensor problem:** a probe unplugged, stuck or reading impossible values
  for 3 minutes, but only once the sensors are **armed** (type `arm` when the
  probes are in the water, so setting up doesn't set off notifications)

To set it up:

1. Install the ntfy app and subscribe to a topic name that's hard to guess,
   e.g. `rivernode-` followed by random letters.
2. Copy `src/secrets_example.h` to `src/secrets.h` and fill in your WiFi
   name, password and the topic. `secrets.h` is in `.gitignore`, so it never
   goes onto GitHub.
3. Upload. You'll get a "River node N1 is online" notification once it
   connects. Type `test` to send another, or `wifi` to check the connection.

The ESP32 only joins **2.4 GHz** WiFi without a login page. A phone hotspot
works (on an iPhone: Settings > Personal Hotspot > turn on "Maximise
Compatibility"). Anyone can watch the alerts arrive without the app by
opening `ntfy.sh/<your topic>` in a browser.

## Several boards (2–3 nodes)

Each board broadcasts its report every minute over **ESP-NOW**, the ESP32's
built-in board-to-board radio. A board that hears a new node adds it to its
river map by itself, so each board only needs to know its own name and place.

Upload each board with its own PlatformIO environment (pick it in the
environment list at the bottom of VS Code, or `pio run -e <name> -t upload`):

| Environment | Name | Distance | Sensors |
|---|---|---|---|
| `node_N0_turbidity` | N0 | 0 m (upstream) | turbidity only |
| `node_N1_full` | N1 | 100 m (middle) | all 4 |
| `node_N2_turbidity` | N2 | 200 m (downstream) | turbidity only |

- Every board needs the same `secrets.h` and must join the **same hotspot**,
  because ESP-NOW only works when every board is on the same WiFi channel.
- Every board sends its own notifications.
- A **turbidity-only** board can't tell sediment from sewage by itself, so it
  shows "Turbidity spike". If a nearby board with all 4 sensors found
  sediment or sewage at the same time, it says so: "it matches N1, 100 m
  upstream, which found Likely sediment, so it's probably the same". It also
  pairs up with that board for the water speed.
- For a believable water speed, pour the muddy water into the cups 30–90 s
  apart.

### Bluetooth page (backup)

The board also shows up over Bluetooth as `RiverNode-N1`. Open the phone page:

**https://barkshavings008.github.io/River-monitoring-nodes/phone/**

- **iPhone:** use the free **Bluefy** browser app (Safari can't use Bluetooth).
- **Android:** use Chrome.

Tap **Connect**, or **Try a demo** to see it without the board. It shows the
live readings, every node, and buttons to arm the sensors or send a test
notification. Keep the page open with the screen on. Bluetooth reaches about
10–30 m.

The page is served by GitHub Pages from the `docs/` folder (Settings > Pages
> Deploy from a branch > `main` / `docs`).

## Simulator

`simulator/index.html` is an interactive map with two streams joining a
river, six nodes and three factories whose waste you choose. It runs the real
node code from `src/`, compiled to WebAssembly. Download the file and open it
in a browser. See [simulator/README.md](simulator/README.md).

## Tests

```
pio test -e native
```

81 unit tests run on a computer: the pollution rules, sensor conversions,
fault checks, learning normal, persistence, the whole engine, the river
network, the water speed, the phone messages and notifications, and the
board-to-board radio packets.

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
- The turbidity noise limits trade false alerts against sensitivity: a
  diluted sewage overflow is usually only caught at the node nearest it.
- The TDS sensor tops out at about 1120 mg/L, so it can't tell brackish
  water from seawater.
- ESP-NOW reaches about 100–200 m in the open. A long river would need
  LoRa radio instead.
- A turbidity-only node can only guess the cause by borrowing it from a
  board with all 4 sensors, and can't run the rain check.
- The water speed is only known after a pollution plume has passed two nodes.
- WiFi notifications need WiFi with internet. Rivers rarely have WiFi, so a
  real deployment would use LoRa radio or a mobile (cellular) connection.
