# River node: manual version

A separate PlatformIO project for writing the river-node logic yourself. The
helper files only translate sensor data and manage the list of nodes. All the
decisions go in `src/main.cpp`.

The original automatic version is untouched in the main `src/` folder.

Open this `manual` folder in VS Code as its own PlatformIO project (File > Open Folder).

## Files

| File | What it's for |
|---|---|
| `src/main.cpp` | Your code. Starts as a short example of the helpers. |
| `src/config.h` | Pins, calibration values, and the `FAKE_SENSORS` switch |
| `src/sensors.h/.cpp` | Reads the sensors and turns them into pH, TDS, NTU and °C |
| `src/river_list.h/.cpp` | Linked list of the river nodes |

`FAKE_SENSORS` in `config.h` is **1** by default. At 1 the sensor functions give
believable made-up readings, so you can test at a desk. Set it to **0** to use
the real sensors.

## Sensor functions (`sensors.h`)

Call `wake_up_sensors()` once in `setup()` first.

| Function | Gives back |
|---|---|
| `return_ph()` | pH, corrected for water temperature |
| `return_tds()` | TDS in mg/L (ppm), corrected to 25 °C using water temperature |
| `return_turbidity()` | Turbidity in NTU (0 = clear) |
| `return_water_temp()` | Water temp in °C (-127 if the sensor is unplugged) |
| `read_all_sensors()` | A `struct Water_reading` with `.ph`, `.tds`, `.turbidity`, `.water_temp`, `.temp_ok` |
| `water_temp_ok(temp)` | `false` if the temperature is -127 or the 85 power-on value |
| `set_fake_readings(ph, tds, ntu, temp)` | Fake mode only: changes what the fake readings are centred on (e.g. pretend there's an acid spill) |
| `return_ph_voltage()` etc. | Raw voltages, for calibrating `PH_V7`, `PH_V4` and `TURB_V_CLEAR` in `config.h` |

The temperature sensor takes about 0.75 s per reading. `return_ph()` and
`return_tds()` each read it, so if you want everything, `read_all_sensors()` is
faster (it only reads the temperature once). If the temperature sensor fails,
the pH and TDS maths use 25 °C instead.

## Linked list functions (`river_list.h`)

Each node in the list is one monitoring station:

```c
struct River_node {
    char id[8];          // e.g. "N1"
    char place[20];      // e.g. "left bank"
    float distance_m;    // distance down the river
    float ph, tds, turbidity, water_temp;
    bool has_reading;
    unsigned long last_update_ms;
    struct River_node *next;   // next node downstream
};
```

The list is always sorted by distance: `head` is the furthest upstream, and
`->next` goes downstream. Any function that can change the first node returns
the new head, so use it like `head = add_node(head, ...);`.

| Function | What it does |
|---|---|
| `head = add_node(head, "N1", 1000, "left bank")` | Adds a node in the right place (ignored if the id is already used) |
| `head = delete_node(head, "N1")` | Removes a node and frees it |
| `head = move_node(head, "N1", 1500)` | Changes a node's distance, keeping its readings |
| `head = delete_all_nodes(head)` | Frees every node (gives back `NULL`) |
| `find_node(head, "N1")` | Pointer to that node, or `NULL` |
| `find_upstream_node(head, node)` | Closest node further upstream, or `NULL` |
| `find_downstream_node(node)` | Closest node further downstream, or `NULL` |
| `count_nodes(head)` | How many nodes there are |
| `distance_between(head, "N0", "N3")` | Metres between two nodes (-1 if one isn't found) |
| `update_readings(node, ph, tds, ntu, temp)` | Saves readings into a node (e.g. from another board) |
| `update_from_sensors(node)` | Reads this board's sensors straight into a node |
| `reading_age_ms(node)` | How long ago the node's readings were updated |
| `print_node(node)` / `print_all_nodes(head)` | Prints a node, or the whole list |

`find_upstream_node()` and `find_downstream_node()` skip any node at the same
distance (e.g. the other bank at the same spot).
