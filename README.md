# Online Status (OBS Plugin)

Show a message or image automatically when your stream has connection issues (dropped frames), and hide it again when things stabilize. Useful to inform viewers you’re experiencing lag or reconnection.

What it does

- Watches your stream’s dropped-frame stats.
- Auto‑shows an overlay when drops exceed a threshold, then auto‑hides after a few stable seconds.
- You choose what to display:
  - Text message (with optional blinking), or
  - An image (with optional blinking).
- Manual “Visible” toggle if you want to show/hide it yourself.

Download

- Get the latest builds from the Releases/Tags page:
  - https://github.com/HectorVzz/online-status/tags
- Download the ZIP for your operating system.

Install

- Windows:
  1. Close OBS.
  2. Unzip the download. Copy the included `obs-plugins`, `bin` and `data` folders into your OBS install (usually `C:\Program Files\obs-studio\`), merging folders if prompted
  3. Start OBS.
- Linux:
  - If the ZIP contains `obs-plugins` and `data`, place them in your OBS install or your OBS portable folder. On many distros, system locations are protected; the simplest is to use a portable OBS or copy into `~/.config/obs-studio/plugins` if the ZIP is structured for user plugins.
  - Alternatively, use your distro’s packaging or build from source (see below).
- macOS:
  - Not available at the moment

Use in OBS

1. In OBS, click the “+” in Sources → add “Online Status”.
2. Pick Content Type: Text or Image.
3. For Text, enter the message. For Image, choose a file.
4. Optional: enable Blink and adjust Blink rate (Hz).
5. Auto‑show settings:
   - “Drop % threshold” — how sensitive the trigger is.
   - “Hide after seconds without drops” — how quickly it disappears once stable.
6. Leave “Visible” on for normal behavior. The overlay will appear only during drops.

### Sections Explained

Dropping:
- Active only while the plugin detects a recent burst of dropped frames above your threshold.
- You pick Text or Image, the drop percentage that triggers it, optional blink, and how long to wait with no further drops before hiding.

Stable:
- Optional “recovery” message that appears after the dropping overlay disappears (i.e., when things stabilized).
- Independent mode (Text/Image), duration timer, and its own blink controls.

Advanced:
- Manual test tools so you can simulate a drop spike or show/hide the stable message without needing real network problems.
- Also contains the manual “Visible” toggle useful for debugging source placement.

Notes

- On Windows the plugin prefers “Text (GDI+)” for text rendering, and falls back to “Text (FreeType 2)” if needed.
- Make sure the built‑in OBS sources “Image” and “Text” are available (they are by default).

Troubleshooting

- Overlay never appears:
  - Ensure you’re streaming (stats are only available while streaming).
  - Lower the “Drop % threshold” temporarily to test.
  - Try typing a short, simple text or pick a small PNG/JPG.
- Nothing renders after switching to Image:
  - Verify the image path is valid and readable.
- Still stuck? Check Help → Log Files → View Last Log File for lines containing “[online-status]”.

Build from source (developers)

- Linux (example)
  ```
  cmake --preset ubuntu-x86_64
  cmake --build --preset ubuntu-x86_64
  cmake --install build_x86_64 --prefix release
  ```
- Windows/macOS builds are provided in Releases via

## Notes by Hector

set preset

```
cmake --preset ubuntu-x86_64
```

Build with next command

```
cmake --build --preset ubuntu-x86_64
```

create release folder

```
cmake --install build_x86_64 --prefix release
```

add message to tell the users the connection is stable now
add how many seconds it will last.
