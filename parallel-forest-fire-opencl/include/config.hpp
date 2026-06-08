#pragma once

#include <string>

namespace fire {
    struct Config {
        int gridSize = 100;
        int steps = 25;
        std::string devicePreference = "gpu";
        float probTree = 0.8f;
        float probBurning = 0.01f;
        float probImmune = 0.3f;
        float probLightning = 0.001f;
    };
}