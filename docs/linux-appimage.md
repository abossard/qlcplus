# Running the Linux AppImage

CI publishes two 64-bit Linux AppImages per build, one per architecture:

| File | Architecture | Built on |
|------|--------------|----------|
| `qlcplus-v5-<version>-<date>-<rev>-x86_64.AppImage` | x86-64 (Intel/AMD) | `ubuntu-24.04` |
| `qlcplus-v5-<version>-<date>-<rev>-aarch64.AppImage` | ARM 64-bit | `ubuntu-24.04-arm` |

```sh
chmod +x qlcplus-v5-*-aarch64.AppImage
./qlcplus-v5-*-aarch64.AppImage
```

## Host packages

The AppImage bundles Qt but not the graphics, audio, and font stack. On a desktop
install these are already present. On a minimal system the app exits 127 with
`error while loading shared libraries: libGLX.so.0`:

```sh
sudo apt install libglx0 libopengl0 libgl1 libegl1 libpulse0 \
                 libbrotli1 libfontconfig1 libglib2.0-0t64 libxkbcommon0
```

Package names above are Debian 13 / Ubuntu 24.04. On x86-64, `libgssapi-krb5-2` is
also required.

## Raspberry Pi

The `aarch64` AppImage targets **Raspberry Pi OS Trixie (64-bit)** and runs on
Raspberry Pi 3, 4, 5, 400/500, CM3-CM5, and Zero 2 W.

It does **not** run on Raspberry Pi OS Bookworm. The build requires `GLIBC_2.38`
and `GLIBCXX_3.4.32`; Bookworm ships glibc 2.36 and GLIBCXX_3.4.30, and fails with:

```
libc.so.6: version `GLIBC_2.38' not found
libstdc++.so.6: version `GLIBCXX_3.4.32' not found
```

Upgrade to Trixie, or build from source. CI enforces the Trixie ABI floors on every
build, so an AppImage that would fail this way never gets published.

32-bit Raspberry Pi OS is not supported; the builds are 64-bit only.

## Running without FUSE

AppImages self-mount through FUSE. Where FUSE is unavailable (containers, some
minimal images), the AppImage exits 127 with `fuse: device not found`. Run it
without mounting:

```sh
APPIMAGE_EXTRACT_AND_RUN=1 ./qlcplus-v5-*-aarch64.AppImage
```

Or unpack it and use the launcher, which sets up the bundled library paths:

```sh
./qlcplus-v5-*-aarch64.AppImage --appimage-extract
./squashfs-root/AppRun
```

Start `squashfs-root/AppRun` rather than `squashfs-root/usr/bin/qlcplus5`; the
binary resolves its bundled Qt relative to the working directory that `AppRun` sets.
