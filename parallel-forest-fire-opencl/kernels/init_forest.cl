// Initial forest generation kernel
uint hash_uint(uint x) {
    x += 0x9e3779b9u;
    x ^= x >> 16;
    x *= 0x85ebca6bu;
    x ^= x >> 13;
    x *= 0xc2b2ae35u;
    x ^= x >> 16;
    return x;
}

__kernel void init_forest(__global int* grid,
                          int totalCells,
                          int n,
                          float probTree,
                          float probBurning,
                          uint seed) {
    int gid = get_global_id(0);

    if (gid >= totalCells) {
        return;
    }

    int row = gid / n;
    int col = gid % n;

    uint cellKey1 = (uint)row * 73856093u ^ (uint)col * 19349663u ^ seed;
    uint x1 = hash_uint(cellKey1);
    float r = (float)x1 / 4294967295.0f;

    uint cellKey2 = (uint)row * 83492791u ^ (uint)col * 29765729u ^ (seed + 1u);
    uint x2 = hash_uint(cellKey2);
    float r2 = (float)x2 / 4294967295.0f;

    if (r < probTree) {
        if (r2 < probBurning) {
            grid[gid] = 2;
        }
        else {
            grid[gid] = 1;
        }
    }
    else {
        grid[gid] = 0;
    }
}