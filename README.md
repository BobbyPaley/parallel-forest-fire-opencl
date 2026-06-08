# Parallel Forest Fire Simulation with OpenCL

A C++ and OpenCL forest-fire cellular automaton simulation comparing execution on OpenCL CPU and GPU devices.

The project models fire spread across an `n × n` forest grid where each cell is one of three states:

- `0` = empty ground
- `1` = tree
- `2` = burning tree

At each simulation step, an OpenCL kernel updates every cell in parallel using the Moore neighbourhood, periodic boundary conditions, probabilistic immunity, and random lightning ignition.

## Project overview

This project was developed as part of a university parallel programming coursework project. The aim was to implement and evaluate a parallel cellular automaton using OpenCL on different compute devices.

The implementation includes:

- C++ host program using OpenCL
- OpenCL kernel for probabilistic forest initialisation
- OpenCL kernel for fire-spread updates
- Double buffering to avoid race conditions between simulation steps
- CPU/GPU device selection
- OpenCL event profiling for kernel timing
- Host-side total runtime timing
- PPM frame export for simulation visualisation
- Python frame viewer
- Python report figure generation from run metadata

## Repository structure

```text
.
├── src/
│   └── main.cpp
├── include/
│   └── config.hpp
├── kernels/
│   ├── init_forest.cl
│   └── spread_fire.cl
├── tools/
│   ├── view_frames.py
│   └── generate_report_figures.py
├── report_assets/
│   └── generated charts are saved here
├── runs/
│   └── simulation output folders are saved here
├── requirements.txt
├── .gitignore
└── README.md
```

## Requirements

This project was developed and tested on Windows using:

- MSYS2 UCRT64
- `g++` from MSYS2 UCRT64
- Khronos OpenCL SDK
- OpenCL CPU runtime / PoCL
- NVIDIA OpenCL platform for GPU runs
- Python 3.x

Python dependencies are listed in `requirements.txt`.

## Build instructions

Run the build command from the repository root in an MSYS2 UCRT64 terminal.

Set `OPENCL_SDK_DIR` to the location of your Khronos OpenCL SDK installation, then run:

```bash
g++ -std=c++17 -Iinclude -I"$OPENCL_SDK_DIR/include" src/main.cpp -o forest_fire.exe -L"$OPENCL_SDK_DIR/lib" -lOpenCL
```

## Running the simulation

```bash
./forest_fire.exe [device] [grid-size] [maximum-steps]
```

Examples:

```bash
./forest_fire.exe gpu 100 25
./forest_fire.exe cpu 400 25
```

The program creates a timestamped run folder inside `runs/`, containing:

- rendered PPM frames
- `metadata.txt`
- `frame_stats.csv`

## Viewing frames

Use the frame viewer on a generated run folder:

```bash
python tools/view_frames.py runs/<run_folder_name>
```

The viewer lets you step through simulation frames and see metadata and per-frame statistics.

## Generating report figures

After collecting CPU and GPU runs, generate tables and charts with:

```bash
python tools/generate_report_figures.py
```

Generated assets are saved in `report_assets/`, including timing comparison and speedup charts.

## Simulation parameters

Default values are defined in `include/config.hpp`:

```cpp
int gridSize = 100;
int steps = 25;
std::string devicePreference = "gpu";
float probTree = 0.8f;
float probBurning = 0.01f;
float probImmune = 0.3f;
float probLightning = 0.001f;
```

## Notes on visualisation

The simulation exports PPM images using the following colour mapping:

- black = empty ground
- green = tree
- orange/red = burning tree

## Skills demonstrated

- Parallel programming
- OpenCL kernel development
- CPU/GPU performance comparison
- Cellular automata modelling
- C++ file handling and timing
- Event profiling
- Python data visualisation
- Experimental evaluation

