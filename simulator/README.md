# River Node Simulator

An interactive map of a river network (two streams joining into a main river)
with six monitoring nodes and three factories. You choose what each factory
dumps into the river and watch the nodes pick it up.

The detecting is done by **the real node code** in `../src`, compiled to
WebAssembly. That's the same code as the ESP32 runs: the sensor conversions, 1-minute medians,
fault checks, pollution rules, persistence, baseline and the network
comparison. The simulator only plays the part of the river and the sensors.

Open `index.html` in a browser (it works on its own, no server needed).

## How it works

```
sim_core.js (the "real world")                     ../src (the node code, as WebAssembly)
------------------------------                     --------------------------------------
river flow, mixing at the join,                    compensation.cpp  voltage -> pH/TDS/NTU/°C
factory waste plumes, mud settling,   voltages     node_engine.cpp   medians, faults, rules,
warm water cooling, rain, algae       ---------->                    persistence, baseline
day-night cycle, sensor noise/faults               network.cpp       compare with other nodes
                                                   display.cpp       serial monitor output
```

- The river is split into 20 m cells. Each minute the water moves along at
  the branch's speed, and at the join the two streams mix by flow.
- A factory's waste mixes into the water passing its outfall, so a bigger
  discharge or a smaller stream means stronger pollution.
- pH is worked out from alkalinity and dissolved carbonate (the river's natural
  buffer). Because of the buffer, a little acid hardly moves the pH, but a lot
  of it drops the pH fast.
- The simulator turns the water at each node into the voltages the real sensors
  would give. That includes the TDS probe's temperature effect and the ESP32's
  3.1 V ADC limit. The voltages go through `sim_add_sample()` in
  `wasm/bridge.cpp`, which does exactly what `read_sensors()` does on the board.

## Files

| File | What it's for |
|---|---|
| `page.html` | The page (map, controls, panels) |
| `sim_core.js` | River, factories, rain, sensors, and the link to the WebAssembly |
| `wasm/bridge.cpp` | Functions the page calls to run the node code |
| `wasm/Arduino.h` | Stand-in for `Serial` that saves the printed text for the page |
| `build.sh` | Compiles `../src` to WebAssembly and puts everything into `index.html` |
| `index.html` | The built page (generated, rebuild after changing `src/`) |

## Rebuilding after changing the node code

```
./simulator/build.sh
```

This needs `clang` (version 16 or newer, with the wasm32 target), `curl` and
`python3`. The first run downloads the WebAssembly C library into
`simulator/.wasi/` (about 3 MB).

## Things to try

- **Start Factory 2 (untreated sewage overflow)**. B1 picks it up, then M1 below
  the join says it's "coming from further upstream at B1".
- **Switch Factory 2 to treated sewage effluent.** The water stays clear, so the
  sewage rule doesn't fire. The effluent / fertiliser rule catches it instead
  (TDS up, pH slightly down, turbidity normal).
- **Start two factories at once.** M1 sees both streams' nodes as its upstream
  neighbours.
- **Rain storm.** Every node sees the rain pattern at once, so it's reported as
  a confirmed weather event and not as pollution.
- **Farm fertiliser runoff.** It shows up straight away as effluent / fertiliser.
  Over the next day an algae bloom makes the pH swing up by day and down at
  night, and the nutrients alert follows (use Skip 12 h).
- **Drag a node** between a factory and the node below it. It relearns normal
  levels for an hour, then narrows down where the source is.
- **Break a sensor** (node panel > Sensors), or take a node offline.
