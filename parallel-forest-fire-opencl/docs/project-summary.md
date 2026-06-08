# Project summary

This project implements a parallel forest-fire cellular automaton using C++ and OpenCL.

The forest is represented as a square grid. Each cell can be empty, contain a tree, or contain a burning tree. At each step, every cell is updated in parallel. Trees may ignite when a neighbouring cell is burning, unless they are immune. Lightning can also randomly ignite trees. Periodic boundary conditions are used so that the grid wraps around at the edges.

The project compares OpenCL CPU and GPU execution using kernel profiling and total runtime measurements. Simulation frames are exported as images so the behaviour can be viewed visually.

## Main features

- OpenCL CPU/GPU execution
- Probabilistic forest initialisation
- Moore neighbourhood fire-spread rule
- Periodic boundary conditions
- Double-buffered simulation updates
- PPM frame output
- Metadata and per-frame statistics
- Python viewer and plotting tools
