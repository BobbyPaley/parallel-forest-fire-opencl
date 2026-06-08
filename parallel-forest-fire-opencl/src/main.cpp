#define CL_HPP_TARGET_OPENCL_VERSION 300
#define CL_HPP_MINIMUM_OPENCL_VERSION 120
#define CL_HPP_ENABLE_EXCEPTIONS

#include <CL/opencl.hpp>
#include "config.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// load kernel file from disk into a string
std::string loadKernelSource(const std::string& filePath) {
    std::ifstream file(filePath);

    if (!file.is_open()) {
        throw std::runtime_error("Could not open kernel file: " + filePath);
    }

    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    );
}

// count current grid states
struct StateCounts {
    int emptyCount = 0;
    int treeCount = 0;
    int burningCount = 0;
};

StateCounts countStates(const std::vector<int>& grid) {
    StateCounts counts;

    for (int value : grid) {
        if (value == 0) {
            ++counts.emptyCount;
        }
        else if (value == 1) {
            ++counts.treeCount;
        }
        else if (value == 2) {
            ++counts.burningCount;
        }
    }

    return counts;
}

// convert OpenCL event duration to milliseconds
double eventToMilliseconds(const cl::Event& event) {
    cl_ulong start = event.getProfilingInfo<CL_PROFILING_COMMAND_START>();
    cl_ulong end = event.getProfilingInfo<CL_PROFILING_COMMAND_END>();
    return static_cast<double>(end - start) / 1e6;
}

// create a unique folder for each run
std::string makeRunFolderName(const fire::Config& config) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &time);
#else
    localtime_r(&time, &localTime);
#endif

    std::ostringstream folder;
    folder << "runs/"
           << config.devicePreference
           << "_n" << config.gridSize
           << "_"
           << std::put_time(&localTime, "%Y%m%d_%H%M%S");

    return folder.str();
}

// create ordered frame names inside a run folder
std::string makeFrameName(const std::string& runFolder, int step) {
    std::ostringstream name;
    name << runFolder << "/frame_" << std::setw(3) << std::setfill('0') << step << ".ppm";
    return name.str();
}

// write basic run metadata at the start
void writeRunMetadataStart(const std::string& runFolder, const fire::Config& config) {
    std::ofstream meta(runFolder + "/metadata.txt");

    if (!meta.is_open()) {
        throw std::runtime_error("Could not open metadata file.");
    }

    meta << "device=" << config.devicePreference << "\n";
    meta << "gridSize=" << config.gridSize << "\n";
    meta << "maxSteps=" << config.steps << "\n";
    meta << "probTree=" << config.probTree << "\n";
    meta << "probBurning=" << config.probBurning << "\n";
    meta << "probImmune=" << config.probImmune << "\n";
    meta << "probLightning=" << config.probLightning << "\n";
}

// append end-of-run metadata
void appendRunMetadataEnd(const std::string& runFolder,
                          int actualSteps,
                          const std::string& terminationReason,
                          double initKernelMs,
                          double totalSpreadKernelMs,
                          double totalProgramMs) {
    std::ofstream meta(runFolder + "/metadata.txt", std::ios::app);

    if (!meta.is_open()) {
        throw std::runtime_error("Could not append to metadata file.");
    }

    meta << "actualSteps=" << actualSteps << "\n";
    meta << "terminationReason=" << terminationReason << "\n";
    meta << "initKernelMs=" << initKernelMs << "\n";
    meta << "totalSpreadKernelMs=" << totalSpreadKernelMs << "\n";
    meta << "totalProgramMs=" << totalProgramMs << "\n";
}

// create CSV for per-frame stats
void initialiseFrameStatsCSV(const std::string& runFolder) {
    std::ofstream csv(runFolder + "/frame_stats.csv");

    if (!csv.is_open()) {
        throw std::runtime_error("Could not create frame stats CSV.");
    }

    csv << "step,empty,tree,burning,changed,proximityIgnition,kernelMs\n";
}

void appendFrameStatsCSV(const std::string& runFolder,
                         int step,
                         const StateCounts& counts,
                         bool changed,
                         int proximityIgnition,
                         double kernelMs) {
    std::ofstream csv(runFolder + "/frame_stats.csv", std::ios::app);

    if (!csv.is_open()) {
        throw std::runtime_error("Could not append frame stats CSV.");
    }

    csv << step << ","
        << counts.emptyCount << ","
        << counts.treeCount << ","
        << counts.burningCount << ","
        << (changed ? 1 : 0) << ","
        << proximityIgnition << ","
        << kernelMs << "\n";
}

// save one grid as a binary PPM image
void saveGridAsPPM(const std::vector<int>& grid, int gridSize, const std::string& filename) {
    std::ofstream out(filename, std::ios::binary);

    if (!out.is_open()) {
        throw std::runtime_error("Could not open output image file: " + filename);
    }

    // automatically scale cell size so the whole grid stays viewable
    const int maxImageSize = 900;
    int cellSize = std::max(1, maxImageSize / gridSize);

    int width = gridSize * cellSize;
    int height = gridSize * cellSize;

    out << "P6\n";
    out << width << ' ' << height << "\n";
    out << "255\n";

    for (int r = 0; r < gridSize; ++r) {
        for (int y = 0; y < cellSize; ++y) {
            for (int c = 0; c < gridSize; ++c) {
                int value = grid[r * gridSize + c];

                unsigned char red = 0;
                unsigned char green = 0;
                unsigned char blue = 0;

                // 0 = empty, 1 = tree, 2 = burning
                if (value == 0) {
                    red = 0; green = 0; blue = 0;
                }
                else if (value == 1) {
                    red = 0; green = 180; blue = 0;
                }
                else if (value == 2) {
                    red = 255; green = 60; blue = 0;
                }

                for (int x = 0; x < cellSize; ++x) {
                    out.write(reinterpret_cast<const char*>(&red), 1);
                    out.write(reinterpret_cast<const char*>(&green), 1);
                    out.write(reinterpret_cast<const char*>(&blue), 1);
                }
            }
        }
    }
}

int main(int argc, char** argv) {
    try {
        fire::Config config;

        if (argc >= 2) config.devicePreference = argv[1];
        if (argc >= 3) config.gridSize = std::stoi(argv[2]);
        if (argc >= 4) config.steps = std::stoi(argv[3]);

        auto totalProgramStart = std::chrono::high_resolution_clock::now();

        // create run folder and metadata file
        std::string runFolder = makeRunFolderName(config);
        std::filesystem::create_directories(runFolder);
        writeRunMetadataStart(runFolder, config);
        initialiseFrameStatsCSV(runFolder);

        std::cout << "=== Forest Fire OpenCL Starter ===\n";
        std::cout << "Requested device: " << config.devicePreference << "\n";
        std::cout << "Grid size: " << config.gridSize << " x " << config.gridSize << "\n";
        std::cout << "Max steps: " << config.steps << "\n";
        std::cout << "Run folder: " << runFolder << "\n\n";

        // discover all OpenCL platforms on the machine
        std::vector<cl::Platform> platforms;
        cl::Platform::get(&platforms);

        if (platforms.empty()) {
            throw std::runtime_error("No OpenCL platforms found.");
        }

        std::cout << "Platforms found: " << platforms.size() << "\n\n";

        // print platform and device information
        for (size_t i = 0; i < platforms.size(); ++i) {
            std::cout << "Platform " << i << ": "
                      << platforms[i].getInfo<CL_PLATFORM_NAME>() << "\n";
            std::cout << "  Vendor: "
                      << platforms[i].getInfo<CL_PLATFORM_VENDOR>() << "\n";
            std::cout << "  Version: "
                      << platforms[i].getInfo<CL_PLATFORM_VERSION>() << "\n";

            std::vector<cl::Device> devices;
            platforms[i].getDevices(CL_DEVICE_TYPE_ALL, &devices);

            for (size_t j = 0; j < devices.size(); ++j) {
                std::cout << "    Device " << j << ": "
                          << devices[j].getInfo<CL_DEVICE_NAME>() << "\n";
            }

            std::cout << "\n";
        }

        // decide whether we want CPU or GPU
        cl_device_type wanted =
            (config.devicePreference == "cpu") ? CL_DEVICE_TYPE_CPU : CL_DEVICE_TYPE_GPU;

        cl::Device selectedDevice;
        bool found = false;

        // select the first matching device from the available platforms
        for (const auto& platform : platforms) {
            std::vector<cl::Device> devices;
            platform.getDevices(wanted, &devices);

            if (!devices.empty()) {
                selectedDevice = devices.front();
                found = true;
                break;
            }
        }

        if (!found) {
            throw std::runtime_error("Requested device type not found.");
        }

        std::cout << "Selected device: "
                  << selectedDevice.getInfo<CL_DEVICE_NAME>() << "\n";

        // create OpenCL context and profiling-enabled command queue
        cl::Context context(selectedDevice);
        cl::CommandQueue queue(context, selectedDevice, CL_QUEUE_PROFILING_ENABLE);

        std::cout << "Context created successfully.\n";
        std::cout << "Profiling-enabled command queue created successfully.\n";


        // load and build init_forest.cl

        std::string initKernelSource = loadKernelSource("kernels/init_forest.cl");
        std::cout << "Loaded kernels/init_forest.cl successfully.\n";

        cl::Program::Sources initSources;
        initSources.push_back({initKernelSource.c_str(), initKernelSource.length()});

        cl::Program initProgram(context, initSources);

        try {
            initProgram.build({selectedDevice});
        }
        catch (...) {
            std::string buildLog = initProgram.getBuildInfo<CL_PROGRAM_BUILD_LOG>(selectedDevice);
            std::cerr << "Build log for init_forest.cl:\n" << buildLog << "\n";
            throw;
        }

        std::cout << "Built init_forest.cl successfully.\n";

        cl::Kernel initKernel(initProgram, "init_forest");
        std::cout << "Created kernel object: init_forest\n";


        // load and build spread_fire.cl

        std::string spreadKernelSource = loadKernelSource("kernels/spread_fire.cl");
        std::cout << "Loaded kernels/spread_fire.cl successfully.\n";

        cl::Program::Sources spreadSources;
        spreadSources.push_back({spreadKernelSource.c_str(), spreadKernelSource.length()});

        cl::Program spreadProgram(context, spreadSources);

        try {
            spreadProgram.build({selectedDevice});
        }
        catch (...) {
            std::string buildLog = spreadProgram.getBuildInfo<CL_PROGRAM_BUILD_LOG>(selectedDevice);
            std::cerr << "Build log for spread_fire.cl:\n" << buildLog << "\n";
            throw;
        }

        std::cout << "Built spread_fire.cl successfully.\n";

        cl::Kernel spreadKernel(spreadProgram, "spread_fire");
        std::cout << "Created kernel object: spread_fire\n";

        std::cout << "\nKernel-loading stage is working.\n";

        // create total number of cells in the n x n grid
        size_t sizeTotal = static_cast<size_t>(config.gridSize) * static_cast<size_t>(config.gridSize);

        // create host grids
        std::vector<int> hostCurrent(sizeTotal, 0);
        std::vector<int> hostNext(sizeTotal, 0);
        std::vector<int> hostReadback(sizeTotal, 0);

        std::cout << "Host grids created successfully.\n";

        // create device buffers
        cl::Buffer deviceCurrent(context, CL_MEM_READ_WRITE, sizeof(int) * sizeTotal);
        cl::Buffer deviceNext(context, CL_MEM_READ_WRITE, sizeof(int) * sizeTotal);
        cl::Buffer deviceProximityFlag(context, CL_MEM_READ_WRITE, sizeof(int));

        std::cout << "Device buffers created successfully.\n";

        // -----------------------------
        // run initialisation kernel
        // -----------------------------
        unsigned int initSeed = 12345;

        initKernel.setArg(0, deviceCurrent);
        initKernel.setArg(1, static_cast<int>(sizeTotal));
        initKernel.setArg(2, config.gridSize);
        initKernel.setArg(3, config.probTree);
        initKernel.setArg(4, config.probBurning);
        initKernel.setArg(5, initSeed);

        cl::Event initEvent;

        queue.enqueueNDRangeKernel(
            initKernel,
            cl::NullRange,
            cl::NDRange(sizeTotal),
            cl::NullRange,
            nullptr,
            &initEvent
        );

        queue.finish();

        double initKernelMs = eventToMilliseconds(initEvent);

        std::cout << "Initialisation kernel completed.\n";

        // read back initial state and save first frame
        std::fill(hostReadback.begin(), hostReadback.end(), 0);

        queue.enqueueReadBuffer(
            deviceCurrent,
            CL_TRUE,
            0,
            sizeof(int) * sizeTotal,
            hostReadback.data()
        );

        saveGridAsPPM(hostReadback, config.gridSize, makeFrameName(runFolder, 0));
        std::cout << "Saved frame: " << makeFrameName(runFolder, 0) << "\n";

        StateCounts initialCounts = countStates(hostReadback);
        appendFrameStatsCSV(runFolder, 0, initialCounts, true, 1, initKernelMs);

        std::cout << "Initial state"
                  << ": empty=" << initialCounts.emptyCount
                  << " tree=" << initialCounts.treeCount
                  << " burning=" << initialCounts.burningCount
                  << " initKernelMs=" << initKernelMs << "\n";

        // keep previous grid so we can detect stability
        std::vector<int> previousGrid = hostReadback;


        // simulation loop
        // stop when:
        // 1) no tree was ignited by proximity in this step
        // 2) no trees left
        // 3) max step limit reached

        int maxSteps = config.steps;
        int actualSteps = 0;
        double totalSpreadKernelMs = 0.0;
        std::string terminationReason = "max_steps_reached";

        while (actualSteps < maxSteps) {
            unsigned int spreadSeed = 54321u + static_cast<unsigned int>(actualSteps);

            // reset proximity flag before each step
            int proximityFlagHost = 0;
            queue.enqueueWriteBuffer(
                deviceProximityFlag,
                CL_TRUE,
                0,
                sizeof(int),
                &proximityFlagHost
            );

            spreadKernel.setArg(0, deviceCurrent);
            spreadKernel.setArg(1, deviceNext);
            spreadKernel.setArg(2, deviceProximityFlag);
            spreadKernel.setArg(3, config.gridSize);
            spreadKernel.setArg(4, static_cast<int>(sizeTotal));
            spreadKernel.setArg(5, config.probImmune);
            spreadKernel.setArg(6, config.probLightning);
            spreadKernel.setArg(7, spreadSeed);

            cl::Event spreadEvent;

            queue.enqueueNDRangeKernel(
                spreadKernel,
                cl::NullRange,
                cl::NDRange(sizeTotal),
                cl::NullRange,
                nullptr,
                &spreadEvent
            );

            queue.finish();

            double spreadKernelMs = eventToMilliseconds(spreadEvent);
            totalSpreadKernelMs += spreadKernelMs;

            // swap current and next buffers so the newly computed state becomes current
            std::swap(deviceCurrent, deviceNext);

            // read back current grid after this step
            std::fill(hostReadback.begin(), hostReadback.end(), 0);

            queue.enqueueReadBuffer(
                deviceCurrent,
                CL_TRUE,
                0,
                sizeof(int) * sizeTotal,
                hostReadback.data()
            );

            // read back proximity flag after this step
            queue.enqueueReadBuffer(
                deviceProximityFlag,
                CL_TRUE,
                0,
                sizeof(int),
                &proximityFlagHost
            );

            ++actualSteps;

            bool changed = (hostReadback != previousGrid);

            saveGridAsPPM(hostReadback, config.gridSize, makeFrameName(runFolder, actualSteps));
            std::cout << "Saved frame: " << makeFrameName(runFolder, actualSteps) << "\n";

            StateCounts counts = countStates(hostReadback);
            appendFrameStatsCSV(runFolder, actualSteps, counts, changed, proximityFlagHost, spreadKernelMs);

            std::cout << "Step " << actualSteps
                    << ": empty=" << counts.emptyCount
                    << " tree=" << counts.treeCount
                    << " burning=" << counts.burningCount
                    << " changed=" << (changed ? 1 : 0)
                    << " proximityIgnition=" << proximityFlagHost
                    << " spreadKernelMs=" << spreadKernelMs << "\n";

            if (counts.treeCount == 0) {
                terminationReason = "no_trees_left";
                std::cout << "Simulation stopped: no trees left.\n";
                break;
            }

            // stop after saving the frame if fire did not spread by proximity
            if (proximityFlagHost == 0) {
                terminationReason = "no_proximity_spread";
                std::cout << "Simulation stopped: no trees ignited by proximity.\n";
                break;
            }

            previousGrid = hostReadback;
        }

        auto totalProgramEnd = std::chrono::high_resolution_clock::now();
        double totalProgramMs =
            std::chrono::duration<double, std::milli>(totalProgramEnd - totalProgramStart).count();

        appendRunMetadataEnd(
            runFolder,
            actualSteps,
            terminationReason,
            initKernelMs,
            totalSpreadKernelMs,
            totalProgramMs
        );

        std::cout << "\nSimulation loop completed after " << actualSteps << " step(s).\n";
        std::cout << "Termination reason: " << terminationReason << "\n";
        std::cout << "Init kernel time (ms): " << initKernelMs << "\n";
        std::cout << "Total spread kernel time (ms): " << totalSpreadKernelMs << "\n";
        std::cout << "Total program time (ms): " << totalProgramMs << "\n";

        return 0;
    }
    catch (const cl::Error& e) {
        std::cerr << "OpenCL error: " << e.what() << " (" << e.err() << ")\n";
        return 1;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}