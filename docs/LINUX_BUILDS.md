# Linux Builds — AppImage Downloads

QLC+ (the QML/v5 variant, `qlcplus-qml`) is built automatically as a
self-contained **AppImage** for multiple Linux architectures on every tagged
release.

## Supported Architectures

| Architecture | Description |
|---|---|
| `x86_64` | Intel / AMD 64-bit (most desktop and server Linux) |
| `aarch64` | ARMv8 64-bit (Raspberry Pi 4/5 in 64-bit mode, Ampere, Apple Silicon under Rosetta/Asahi, etc.) |
| `armv7` | ARMv7 hard-float *(manual dispatch only — see notes below)* |

## Downloading an AppImage

1. Go to the [**Releases** page](../../releases) of this repository.
2. Expand the **Assets** section of the latest release.
3. Download the AppImage that matches your architecture, for example:
   - `QLC+-5.2.2-GIT-abcdef1-x86_64.AppImage`
   - `QLC+-5.2.2-GIT-abcdef1-aarch64.AppImage`
4. Optionally download the matching `.sha256` file to verify integrity.

## Verifying the Download

```bash
sha256sum --check QLC+-5.2.2-GIT-abcdef1-x86_64.AppImage.sha256
```

## Running the AppImage

```bash
# Make it executable (only needed once)
chmod +x QLC+-5.2.2-GIT-abcdef1-x86_64.AppImage

# Run it
./QLC+-5.2.2-GIT-abcdef1-x86_64.AppImage
```

### Without FUSE (fallback)

AppImages normally require FUSE to mount themselves.  If FUSE is unavailable
(e.g. inside a container or on a stripped-down server image), you can still run
the AppImage by extracting it first:

```bash
# Extract the AppImage into a folder called "squashfs-root"
./QLC+-5.2.2-GIT-abcdef1-x86_64.AppImage --appimage-extract

# Run the extracted binary
./squashfs-root/AppRun
```

Or set the environment variable before running:

```bash
APPIMAGE_EXTRACT_AND_RUN=1 ./QLC+-5.2.2-GIT-abcdef1-x86_64.AppImage
```

## Runtime Requirements

| Requirement | Minimum version |
|---|---|
| Linux kernel | 3.10+ (5.x+ recommended) |
| glibc | 2.35 (Ubuntu 22.04 baseline) |
| FUSE 2 | Required for normal AppImage execution (`libfuse2` on Debian/Ubuntu) — or use `--appimage-extract-and-run` |
| Display | X11 or Wayland (via XWayland) |

No Qt installation is required — all Qt libraries and QML modules are bundled
inside the AppImage.

## Installing FUSE 2 (if needed)

```bash
# Debian / Ubuntu
sudo apt-get install libfuse2

# Fedora / RHEL
sudo dnf install fuse-libs

# Arch Linux
sudo pacman -S fuse2
```

## Notes on armv7

armv7 (32-bit ARM) builds are gated behind a manual `workflow_dispatch` trigger
because:

- GitHub does not offer hosted armv7 runners.
- Qt 6 for Linux armv7 is only available as Qt 6.2.x (Ubuntu 22.04 apt).
- Full QEMU emulation of the build takes 45–90 minutes, exceeding typical
  free-tier CI time limits.

If you need armv7 packages, consider building on a native Raspberry Pi or
self-hosted ARM runner.  armv7 artifacts are distributed as a `.tar.gz`
tarball rather than an AppImage (no `linuxdeploy` armhf AppImage tool is
publicly available).

## Build System Details

The CI workflow lives at
[`.github/workflows/linux-build.yml`](../.github/workflows/linux-build.yml).

Key choices:

| Aspect | x86_64 | aarch64 |
|---|---|---|
| Runner | `ubuntu-22.04` | `ubuntu-22.04-arm` |
| Qt installation | `jurplel/install-qt-action` (Qt 6.9.3) | System apt Qt 6 |
| Library bundling | `linuxdeploy` + `linuxdeploy-plugin-qt` | `linuxdeploy` + `linuxdeploy-plugin-qt` |
| Output | AppImage | AppImage |
