# Retro screensavers

Two animated pixel-art scenes for Windows, written in C and built on Linux. Each is a separate screen saver in the Windows picker. The artwork is drawn in code, so the two `.scr` files are all you need to copy.

| Glasswater Aquarium | Cassini Drift |
| --- | --- |
| ![Angelfish and a discus swim above planted driftwood and a gravel bed](previews/aquarium.png) | ![A survey probe passes a banded planet and its inclined rings](previews/space.png) |
| Angelfish and discus wander through a planted freshwater tank. Schools of tetras pass at different depths, bubbles rise, and the current bends the leaves. | A survey probe crosses a slowly turning ringed world. Follow the cratered moon around its orbit and watch the planet cast a shadow across the rings. |

## Build on Linux

On Debian, Ubuntu, or Ubuntu under WSL:

```sh
sudo apt update
sudo apt install make gcc-mingw-w64-x86-64 binutils-mingw-w64-x86-64
git clone https://github.com/jsdyer/screensaver.git
cd screensaver
make
```

This produces **`build/aquarium.scr`** and **`build/space.scr`** for 64-bit Windows. The Makefile uses the same MinGW-w64 compiler and Win32 GDI approach as StarfieldSaver. `windres` embeds each saver's display name and settings dialog. Run `make build/space.scr` to build just the space scene.

Prebuilt copies are available from [Build screensavers](https://github.com/jsdyer/screensaver/actions/workflows/build.yml). Open a successful run and download the **windows-screensavers-x64** artifact.

## Try them on Windows

Copy the two files to your PC. To try one immediately, open PowerShell in that folder and run:

```powershell
.\aquarium.scr /s
.\space.scr /s
```

Run one command at a time. Move the mouse or press a key to exit. For both scenes to appear in the Windows picker, copy both files to `C:\Windows\System32` (administrator access required), search Start for **Change screen saver**, and select a scene. **Settings** offers slow, normal, and brisk animation. The choice is saved per scene for your Windows account.

The same executable handles the Windows switches used by StarfieldSaver:

| Switch | Behaviour |
| --- | --- |
| `/s` | Full screen. Monitors share one frame. Narrower monitors show a centre crop to preserve the artwork's proportions. |
| `/p HWND` or `/p:HWND` | Renders into the small preview window owned by Screen Saver Settings. |
| `/c`, `/c:HWND`, or no switch | Opens that scene's settings. |

## Sign the screensavers on AlmaLinux

Each `.scr` is a Windows executable. You can use OpenSSL to create a free practice certificate and `osslsigncode` to embed an Authenticode signature in each saver. Start in the project root with `build/aquarium.scr` and `build/space.scr` present. You can use either your own build or the two files extracted from the GitHub Actions ZIP.

### Install the signing tool

Check your release with `cat /etc/almalinux-release` and your CPU architecture with `uname -m`.

**AlmaLinux 9** has an EPEL package. [Fedora package listing](https://packages.fedoraproject.org/pkgs/osslsigncode/osslsigncode/)

```sh
sudo dnf install epel-release
sudo dnf install openssl osslsigncode
```

**AlmaLinux 10 on x86-64** can use the upstream binary. This installs version 2.16 under your own account after checking the archive's published SHA-256. [Upstream release and checksums](https://github.com/mtrojnar/osslsigncode/releases/tag/2.16)

```sh
sudo dnf install openssl
(
  set -e
  cd "$(mktemp -d)"
  asset=osslsigncode-2.16-linux-amd64
  curl -fL "https://github.com/mtrojnar/osslsigncode/releases/download/2.16/$asset.tar.gz" -o signer.tgz
  echo '109c711ac07fc2ce1e3756d6247d92bbf23e55c4aa809a4b34cca3c4f2913fc6  signer.tgz' | sha256sum -c -
  tar -xzf signer.tgz
  install -Dm755 "$asset/osslsigncode" "$HOME/.local/bin/osslsigncode"
)
export PATH="$HOME/.local/bin:$PATH"
```

### Sign and verify both files

Create a seven-day practice certificate. If you already created `lab.key` and `lab.crt`, use that directory as `$lab` and start with the signing loop.

```sh
lab=$(mktemp -d)
openssl req -x509 -newkey rsa:3072 -nodes -sha256 -days 7 \
  -subj '/CN=Screensaver Signing Lab' \
  -addext 'basicConstraints=critical,CA:FALSE' \
  -addext 'keyUsage=critical,digitalSignature' \
  -addext 'extendedKeyUsage=codeSigning' \
  -keyout "$lab/lab.key" -out "$lab/lab.crt"
```

```sh
(
  set -e
  for scene in aquarium space; do
    osslsigncode sign -certs "$lab/lab.crt" -key "$lab/lab.key" -h sha256 \
      -in "build/$scene.scr" -out "$lab/$scene.scr"
    osslsigncode verify -CAfile "$lab/lab.crt" -in "$lab/$scene.scr"
  done
  printf 'Signed copies are in %s\n' "$lab"
)
```

Look for `Signature verification: ok` for each file. The `-CAfile` argument tells this invocation to trust your lab certificate. Copy only the two `.scr` files from the printed directory to Windows. Keep `lab.key` private. To inspect a copy in PowerShell, run:

```powershell
Get-AuthenticodeSignature .\aquarium.scr |
    Format-List Status, StatusMessage, SignerCertificate
```

The lab copy should show a signer with the subject `CN=Screensaver Signing Lab`. On a PC with the default trust settings, Windows can read that signature and still report that it doesn't trust the certificate. A status of `NotSigned` is a reason to check whether you copied the original from `build/`.

### Public signing and the Windows warning

Windows can show **“Windows protected your PC”** for a new download even when it displays a verified publisher. Authenticode establishes who signed the file and whether its signed contents are intact. SmartScreen separately evaluates the reputation of the publisher and the downloaded file. Extended Validation (EV) also goes through that reputation check. [Microsoft's SmartScreen guide](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation)

| Option | Signing key and cost | Publisher on a typical Windows PC |
| --- | --- | --- |
| Self-signed lab certificate | A local key file. Free to create and reuse. | Your chosen name is in the signature. Public Windows installations have no independent trust in that name. |
| Azure Artifact Signing, Public Trust | Microsoft verifies the company and operates the protected key. Basic is US$9.99/month for 5,000 signing operations. | The validated company. One signing identity can cover several products and releases. [Azure pricing](https://azure.microsoft.com/en-ca/products/artifact-signing/) |
| Commercial certificate from DigiCert or Sectigo | A paid certificate with the key in an approved USB token or hardware security module. Hosted signing plans have their own quotas and prices. | The validated company. A token lets you authorize signing on a machine you control. [DigiCert options](https://www.digicert.com/signing/compare-code-signing-certificates) · [Sectigo options](https://www.sectigo.com/enterprise-solutions/certificate-manager/code-signing-solutions) |
| SignPath Foundation | Free hosted signing for accepted open-source projects and their approved builds. | SignPath Foundation is the publisher when using the Foundation's certificate. [Eligibility and terms](https://signpath.org/terms) |

With Azure, signing the aquarium once and the space scene once uses **two signatures**. Ten thousand downloads of the resulting ZIP use **zero additional signatures**. Signing both files for the next release uses two more. For public releases, sign and timestamp the final executables before packaging the ZIP. A trusted timestamp lets Windows check that the certificate was valid when the file was signed. [Azure signing FAQ](https://learn.microsoft.com/en-us/azure/artifact-signing/faq) · [Authenticode timestamps](https://learn.microsoft.com/en-us/windows/win32/seccrypto/time-stamping-authenticode-signatures)

## Read and extend the code

Start at **`scene_draw()`**, at the bottom of [aquarium.c](src/aquarium.c) or [space.c](src/space.c). The order of the calls is the order you see on screen. In the aquarium, for instance, the last few clumps of leaves can hide a fish as it swims past them.

- [canvas.h](src/canvas.h) and [canvas.c](src/canvas.c) provide pixels, lines, filled rectangles, ovals, and colour blending. Coordinates start at the top left. Colours use `0xRRGGBB`.
- [saver.c](src/saver.c) handles Windows, timing, input, and scaling. It draws at 320 logical pixels high with square pixels, then scales the finished frame to the screen. Both scenes share this file.
- [scene.h](src/scene.h) is the whole interface a new scene needs. The host supplies the canvas, elapsed animation time in seconds, and a seed that stays fixed for the run. Cache expensive scenery while the dimensions and seed stay the same.

To add a third saver, create `src/example.c` with a `scene_draw()` function that paints the whole frame. Copy `src/space.rc` to `src/example.rc` and change its display name. Add `example` to `SCENES` in the [Makefile](Makefile), then run `make` to produce `build/example.scr`.

The original StarfieldSaver license notice is included in [NOTICE](NOTICE).
