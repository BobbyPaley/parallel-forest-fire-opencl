import os
import math
import statistics
import matplotlib.pyplot as plt

RUNS_DIR = "runs"
OUTPUT_DIR = "report_assets"

TABLE_IMAGE = os.path.join(OUTPUT_DIR, "results_table.png")
TIMING_GRAPH_IMAGE = os.path.join(OUTPUT_DIR, "timing_graph.png")
SPEEDUP_GRAPH_IMAGE = os.path.join(OUTPUT_DIR, "speedup_graph.png")


# Read key-value pairs from a metadata file
def parse_metadata_file(filepath):
    data = {}

    with open(filepath, "r", encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if "=" in line:
                key, value = line.split("=", 1)
                data[key.strip()] = value.strip()

    return data


# Convert a value to float safely
def safe_float(value, default=0.0):
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


# Convert a value to int safely
def safe_int(value, default=0):
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


# Collect metadata from all run folders
def collect_runs():
    rows = []

    if not os.path.isdir(RUNS_DIR):
        raise FileNotFoundError(f"Could not find '{RUNS_DIR}' folder.")

    for folder_name in os.listdir(RUNS_DIR):
        folder_path = os.path.join(RUNS_DIR, folder_name)

        if not os.path.isdir(folder_path):
            continue

        metadata_path = os.path.join(folder_path, "metadata.txt")
        if not os.path.isfile(metadata_path):
            continue

        meta = parse_metadata_file(metadata_path)

        row = {
            "runFolder": folder_name,
            "device": meta.get("device", "").lower(),
            "gridSize": safe_int(meta.get("gridSize")),
            "maxSteps": safe_int(meta.get("maxSteps")),
            "actualSteps": safe_int(meta.get("actualSteps")),
            "probTree": safe_float(meta.get("probTree")),
            "probBurning": safe_float(meta.get("probBurning")),
            "probImmune": safe_float(meta.get("probImmune")),
            "probLightning": safe_float(meta.get("probLightning")),
            "terminationReason": meta.get("terminationReason", ""),
            "initKernelMs": safe_float(meta.get("initKernelMs")),
            "totalSpreadKernelMs": safe_float(meta.get("totalSpreadKernelMs")),
            "totalProgramMs": safe_float(meta.get("totalProgramMs")),
        }

        rows.append(row)

    if not rows:
        raise RuntimeError("No valid run metadata found in 'runs/'.")

    rows.sort(key=lambda r: (r["gridSize"], r["device"], r["runFolder"]))
    return rows


# Return the average of a list
def mean(values):
    return statistics.mean(values) if values else 0.0


# Return standard deviation when possible
def stdev(values):
    return statistics.stdev(values) if len(values) >= 2 else 0.0


# Group runs by grid size and device
def group_runs(rows):
    grouped = {}

    for row in rows:
        key = (row["gridSize"], row["device"])
        grouped.setdefault(key, []).append(row)

    summary = []

    for (grid_size, device), group in sorted(grouped.items()):
        init_values = [r["initKernelMs"] for r in group]
        spread_values = [r["totalSpreadKernelMs"] for r in group]
        total_values = [r["totalProgramMs"] for r in group]
        step_values = [r["actualSteps"] for r in group]

        termination_counts = {}
        for r in group:
            reason = r["terminationReason"]
            termination_counts[reason] = termination_counts.get(reason, 0) + 1

        most_common_termination = max(termination_counts, key=termination_counts.get)

        summary.append({
            "gridSize": grid_size,
            "device": device,
            "runs": len(group),
            "meanInitMs": mean(init_values),
            "stdInitMs": stdev(init_values),
            "meanSpreadMs": mean(spread_values),
            "stdSpreadMs": stdev(spread_values),
            "meanTotalMs": mean(total_values),
            "stdTotalMs": stdev(total_values),
            "meanSteps": mean(step_values),
            "terminationReason": most_common_termination
        })

    return summary


# Save grouped results as a table image
def create_table_image(summary_rows):
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    headers = [
        "Grid Size",
        "Device",
        "Runs",
        "Mean Init (ms)",
        "Mean Spread (ms)",
        "Mean Total (ms)",
        "Std Total (ms)",
        "Mean Steps",
        "Termination"
    ]

    table_rows = []
    for r in summary_rows:
        table_rows.append([
            f"{r['gridSize']}×{r['gridSize']}",
            r["device"].upper(),
            str(r["runs"]),
            f"{r['meanInitMs']:.3f}",
            f"{r['meanSpreadMs']:.3f}",
            f"{r['meanTotalMs']:.3f}",
            f"{r['stdTotalMs']:.3f}",
            f"{r['meanSteps']:.2f}",
            r["terminationReason"]
        ])

    fig_height = max(4, 0.5 * len(table_rows) + 1.5)
    fig, ax = plt.subplots(figsize=(16, fig_height))
    ax.axis("off")

    table = ax.table(
        cellText=table_rows,
        colLabels=headers,
        loc="center",
        cellLoc="center"
    )

    table.auto_set_font_size(False)
    table.set_fontsize(10)
    table.scale(1, 1.6)

    ax.set_title("Forest Fire Simulation Results (Grouped by Device and Grid Size)", fontsize=14, pad=20)

    plt.tight_layout()
    plt.savefig(TABLE_IMAGE, dpi=200, bbox_inches="tight")
    plt.close(fig)


# Save total runtime comparison graph
def create_timing_graph(summary_rows):
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    cpu_rows = [r for r in summary_rows if r["device"] == "cpu"]
    gpu_rows = [r for r in summary_rows if r["device"] == "gpu"]

    cpu_rows.sort(key=lambda r: r["gridSize"])
    gpu_rows.sort(key=lambda r: r["gridSize"])

    fig, ax = plt.subplots(figsize=(10, 6))

    if cpu_rows:
        ax.errorbar(
            [r["gridSize"] for r in cpu_rows],
            [r["meanTotalMs"] for r in cpu_rows],
            yerr=[r["stdTotalMs"] for r in cpu_rows],
            marker="o",
            capsize=4,
            label="CPU Total Time"
        )

    if gpu_rows:
        ax.errorbar(
            [r["gridSize"] for r in gpu_rows],
            [r["meanTotalMs"] for r in gpu_rows],
            yerr=[r["stdTotalMs"] for r in gpu_rows],
            marker="o",
            capsize=4,
            label="GPU Total Time"
        )

    ax.set_title("CPU vs GPU Total Execution Time")
    ax.set_xlabel("Grid Size (n × n)")
    ax.set_ylabel("Mean Total Program Time (ms)")
    ax.grid(True)
    ax.legend()

    plt.tight_layout()
    plt.savefig(TIMING_GRAPH_IMAGE, dpi=200, bbox_inches="tight")
    plt.close(fig)

# Save spread kernel timing graph
def create_spread_graph(summary_rows):
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    spread_graph_image = os.path.join(OUTPUT_DIR, "spread_kernel_graph.png")

    cpu_rows = [r for r in summary_rows if r["device"] == "cpu"]
    gpu_rows = [r for r in summary_rows if r["device"] == "gpu"]

    cpu_rows.sort(key=lambda r: r["gridSize"])
    gpu_rows.sort(key=lambda r: r["gridSize"])

    fig, ax = plt.subplots(figsize=(10, 6))

    if cpu_rows:
        ax.errorbar(
            [r["gridSize"] for r in cpu_rows],
            [r["meanSpreadMs"] for r in cpu_rows],
            yerr=[r["stdSpreadMs"] for r in cpu_rows],
            marker="o",
            capsize=4,
            label="CPU Spread Kernel Time"
        )

    if gpu_rows:
        ax.errorbar(
            [r["gridSize"] for r in gpu_rows],
            [r["meanSpreadMs"] for r in gpu_rows],
            yerr=[r["stdSpreadMs"] for r in gpu_rows],
            marker="o",
            capsize=4,
            label="GPU Spread Kernel Time"
        )

    ax.set_title("CPU vs GPU Spread Kernel Time")
    ax.set_xlabel("Grid Size (n × n)")
    ax.set_ylabel("Mean Spread Kernel Time (ms)")
    ax.grid(True)
    ax.legend()

    plt.tight_layout()
    plt.savefig(spread_graph_image, dpi=200, bbox_inches="tight")
    plt.close(fig)

# Save GPU speedup comparison graph
def create_speedup_graph(summary_rows):
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    by_size = {}
    for r in summary_rows:
        by_size.setdefault(r["gridSize"], {})
        by_size[r["gridSize"]][r["device"]] = r

    sizes = []
    speedups = []

    for grid_size in sorted(by_size.keys()):
        group = by_size[grid_size]
        if "cpu" in group and "gpu" in group:
            cpu_time = group["cpu"]["meanTotalMs"]
            gpu_time = group["gpu"]["meanTotalMs"]

            if gpu_time > 0:
                sizes.append(grid_size)
                speedups.append(cpu_time / gpu_time)

    if not sizes:
        return

    fig, ax = plt.subplots(figsize=(10, 6))
    ax.plot(sizes, speedups, marker="o")
    ax.set_title("GPU Speedup over CPU")
    ax.set_xlabel("Grid Size (n × n)")
    ax.set_ylabel("Speedup (Mean CPU Time / Mean GPU Time)")
    ax.grid(True)

    plt.tight_layout()
    plt.savefig(SPEEDUP_GRAPH_IMAGE, dpi=200, bbox_inches="tight")
    plt.close(fig)


# Print a short console summary
def print_summary(summary_rows):
    print("\nGrouped results:")
    for r in summary_rows:
        print(
            f"{r['gridSize']}x{r['gridSize']} | "
            f"{r['device'].upper()} | "
            f"runs={r['runs']} | "
            f"mean total={r['meanTotalMs']:.3f} ms | "
            f"std total={r['stdTotalMs']:.3f} ms | "
            f"mean spread={r['meanSpreadMs']:.3f} ms"
        )

    print("\nCreated:")
    print(f"  {TABLE_IMAGE}")
    print(f"  {TIMING_GRAPH_IMAGE}")
    print(f"  {SPEEDUP_GRAPH_IMAGE}")


# Generate all report figures
def main():
    rows = collect_runs()
    summary_rows = group_runs(rows)

    create_table_image(summary_rows)
    create_timing_graph(summary_rows)
    create_speedup_graph(summary_rows)
    print_summary(summary_rows)
    create_spread_graph(summary_rows)


if __name__ == "__main__":
    main()