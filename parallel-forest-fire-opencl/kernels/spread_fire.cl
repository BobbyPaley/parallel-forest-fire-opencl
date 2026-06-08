// Fire spread update kernel
uint hash_uint(uint x) {
    x += 0x9e3779b9u;
    x ^= x >> 16;
    x *= 0x85ebca6bu;
    x ^= x >> 13;
    x *= 0xc2b2ae35u;
    x ^= x >> 16;
    return x;
}

__kernel void spread_fire(__global const int* currentGrid,
                          __global int* nextGrid,
                          __global int* proximityFlag,
                          int n,
                          int totalCells,
                          float probImmune,
                          float probLightning,
                          uint seed) {
    int gid = get_global_id(0);

    if (gid >= totalCells) {
        return;
    }

    int row = gid / n;
    int col = gid % n;

    bool hasBurningNeighbour = false;

    for (int dr = -1; dr <= 1; ++dr) {
        for (int dc = -1; dc <= 1; ++dc) {
            if (dr == 0 && dc == 0) {
                continue;
            }

            int nr = (row + dr + n) % n;
            int nc = (col + dc + n) % n;
            int neighbourIndex = nr * n + nc;

            if (currentGrid[neighbourIndex] == 2) {
                hasBurningNeighbour = true;
            }
        }
    }

    int currentState = currentGrid[gid];

    // empty stays empty
    if (currentState == 0) {
        nextGrid[gid] = 0;
    }
    // burning becomes empty
    else if (currentState == 2) {
        nextGrid[gid] = 0;
    }
    // tree logic
    else if (currentState == 1) {
        uint cellKey1 = (uint)row * 73856093u ^ (uint)col * 19349663u ^ seed;
        uint x1 = hash_uint(cellKey1);
        float rImmune = (float)x1 / 4294967295.0f;

        uint cellKey2 = (uint)row * 83492791u ^ (uint)col * 29765729u ^ (seed + 1u);
        uint x2 = hash_uint(cellKey2);
        float rLightning = (float)x2 / 4294967295.0f;

        if (rImmune < probImmune) {
            nextGrid[gid] = 1;
        }
        else {
            // if ignited by proximity, set the flag
            if (hasBurningNeighbour) {
                nextGrid[gid] = 2;
                atomic_or(proximityFlag, 1);
            }
            else if (rLightning < probLightning) {
                nextGrid[gid] = 2;
            }
            else {
                nextGrid[gid] = 1;
            }
        }
    }
}