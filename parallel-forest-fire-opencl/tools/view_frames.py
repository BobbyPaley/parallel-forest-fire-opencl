import csv
import os
import sys
import tkinter as tk
from tkinter import messagebox

# basic config
FRAME_DIR = sys.argv[1] if len(sys.argv) > 1 else "frames"
FRAME_EXT = ".ppm"
PLAY_DELAY_MS = 500


# Handles frame display and controls
class FrameViewer:
    # Build the viewer window
    def __init__(self, root):
        self.root = root
        self.root.title("Forest Fire Frame Viewer")

        # load frames
        self.frames = self.loadFrames()
        if not self.frames:
            messagebox.showerror("No frames found", f"No {FRAME_EXT} files found in '{FRAME_DIR}'")
            root.destroy()
            return

        # load run info
        self.metadata = self.loadMetadata()
        self.frameStats = self.loadFrameStats()

        self.index = 0
        self.playing = False
        self.after_id = None
        self.image = None

        # main layout
        mainFrame = tk.Frame(root)
        mainFrame.pack(fill="both", expand=True)

        leftPanel = tk.Frame(mainFrame)
        leftPanel.pack(side="left", fill="both", expand=True, padx=10, pady=10)

        rightPanel = tk.Frame(mainFrame, padx=10, pady=10)
        rightPanel.pack(side="right", fill="y")

        # image display
        self.label = tk.Label(leftPanel)
        self.label.pack(padx=10, pady=10)

        # frame label
        self.infoLabel = tk.Label(leftPanel, text="", font=("Arial", 12))
        self.infoLabel.pack()

        # controls moved to right panel
        tk.Label(rightPanel, text="Controls", font=("Arial", 12, "bold")).pack(anchor="w")

        buttonFrame = tk.Frame(rightPanel)
        buttonFrame.pack(fill="x", pady=(0, 15))

        self.prevButton = tk.Button(buttonFrame, text="<< Prev", width=10, command=self.prevFrame)
        self.prevButton.grid(row=0, column=0, padx=5, pady=5)

        self.playButton = tk.Button(buttonFrame, text="Play", width=10, command=self.togglePlay)
        self.playButton.grid(row=0, column=1, padx=5, pady=5)

        self.nextButton = tk.Button(buttonFrame, text="Next >>", width=10, command=self.nextFrame)
        self.nextButton.grid(row=0, column=2, padx=5, pady=5)

        # metadata display
        tk.Label(rightPanel, text="Run Metadata", font=("Arial", 12, "bold")).pack(anchor="w")
        self.metadataText = tk.Text(rightPanel, width=40, height=14, wrap="word")
        self.metadataText.pack(fill="x", pady=(0, 10))
        self.metadataText.insert("1.0", self.formatMetadata())
        self.metadataText.config(state="disabled")

        # frame stats display
        tk.Label(rightPanel, text="Frame Stats", font=("Arial", 12, "bold")).pack(anchor="w")
        self.statsText = tk.Text(rightPanel, width=40, height=12, wrap="word")
        self.statsText.pack(fill="x")

        # keyboard shortcuts
        self.root.bind("<Left>", lambda event: self.prevFrame())
        self.root.bind("<Right>", lambda event: self.nextFrame())
        self.root.bind("<space>", lambda event: self.togglePlay())

        self.showFrame()

    # Load all saved frame images
    def loadFrames(self):
        if not os.path.isdir(FRAME_DIR):
            return []

        files = [
            os.path.join(FRAME_DIR, f)
            for f in os.listdir(FRAME_DIR)
            if f.lower().endswith(FRAME_EXT)
        ]
        files.sort()
        return files

    # Load run metadata
    def loadMetadata(self):
        metadata = {}
        metadataPath = os.path.join(FRAME_DIR, "metadata.txt")

        if not os.path.isfile(metadataPath):
            return metadata

        with open(metadataPath, "r", encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if "=" in line:
                    key, value = line.split("=", 1)
                    metadata[key] = value

        return metadata

    # Load per-frame statistics
    def loadFrameStats(self):
        stats = {}
        csvPath = os.path.join(FRAME_DIR, "frame_stats.csv")

        if not os.path.isfile(csvPath):
            return stats

        with open(csvPath, "r", encoding="utf-8", newline="") as f:
            reader = csv.DictReader(f)
            for row in reader:
                step = int(row["step"])
                stats[step] = row

        return stats

    # Format metadata for display
    def formatMetadata(self):
        if not self.metadata:
            return "No metadata found."

        preferredOrder = [
            "device",
            "gridSize",
            "maxSteps",
            "actualSteps",
            "terminationReason",
            "probTree",
            "probBurning",
            "probImmune",
            "probLightning",
            "initKernelMs",
            "totalSpreadKernelMs",
            "totalProgramMs",
        ]

        lines = []
        for key in preferredOrder:
            if key in self.metadata:
                lines.append(f"{key}: {self.metadata[key]}")

        for key, value in self.metadata.items():
            if key not in preferredOrder:
                lines.append(f"{key}: {value}")

        return "\n".join(lines)

    # Show the selected frame
    def showFrame(self):
        framePath = self.frames[self.index]
        self.image = tk.PhotoImage(file=framePath)

        self.label.config(image=self.image)
        self.infoLabel.config(
            text=f"Frame {self.index} / {len(self.frames) - 1}   ({os.path.basename(framePath)})"
        )

        self.updateFrameStats()

    # Show stats for the selected frame
    def updateFrameStats(self):
        step = self.index
        self.statsText.config(state="normal")
        self.statsText.delete("1.0", "end")

        if step in self.frameStats:
            row = self.frameStats[step]

            text = (
                f"step: {row.get('step', '')}\n"
                f"empty: {row.get('empty', '')}\n"
                f"tree: {row.get('tree', '')}\n"
                f"burning: {row.get('burning', '')}\n"
                f"changed: {row.get('changed', '')}\n"
                f"kernelMs: {row.get('kernelMs', '')}\n"
            )

            if "proximityIgnition" in row:
                text += f"proximityIgnition: {row.get('proximityIgnition', '')}\n"

            if "changedCells" in row:
                text += f"changedCells: {row.get('changedCells', '')}\n"
        else:
            text = f"No frame stats found for step {step}."

        self.statsText.insert("1.0", text)
        self.statsText.config(state="disabled")

    # Move to the next frame
    def nextFrame(self):
        self.index = (self.index + 1) % len(self.frames)
        self.showFrame()

    # Move to the previous frame
    def prevFrame(self):
        self.index = (self.index - 1) % len(self.frames)
        self.showFrame()

    # Start or stop autoplay
    def togglePlay(self):
        self.playing = not self.playing
        self.playButton.config(text="Pause" if self.playing else "Play")

        if self.playing:
            self.scheduleNext()
        else:
            if self.after_id is not None:
                self.root.after_cancel(self.after_id)
                self.after_id = None

    # Advance frames during autoplay
    def scheduleNext(self):
        self.nextFrame()
        if self.playing:
            self.after_id = self.root.after(PLAY_DELAY_MS, self.scheduleNext)


if __name__ == "__main__":
    root = tk.Tk()
    app = FrameViewer(root)
    root.mainloop()
