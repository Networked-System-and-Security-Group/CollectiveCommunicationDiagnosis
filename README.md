# Vedrfolnir

This code repository contains the source code for Vedrfolnir's NS3 simulation. It is based on [NS-3 simulator for Hawkeye](https://github.com/hawkeye-anonymous/Hawkeye). 

## Quick Start

### Build

```
cd Vedrfolnir
./waf configure
```

Please note if gcc version > 5, compilation will fail due to some ns3 code style. If this what you encounter, please use:

```
CC='gcc-5' CXX='g++-5' ./waf configure
```

### Experiment setup

Please see `Vedrfolnir/mix_allreduce/config.txt` for example.

### Run

```
./waf --run 'scratch/mix_allreduce.cc mix_allreduce/config.txt'
```

This will run a diagnostic example of ring allreduce. You can read `mix_allreduce.cc` in detail to understand the main logic of the simulation.

During the run, data output from the host side is logged in `mix_allreduce/out/fct.txt` and telemetry data output from the switch is logged in `mix_allreduce/data`. After the run of the simulation, in `mix_allreduce/data` directory, execute `python demo_client.py` to build the waiting graph, and `python graph.py`  `python demo_telemetry.py` sequentially to build the network provenance graph.