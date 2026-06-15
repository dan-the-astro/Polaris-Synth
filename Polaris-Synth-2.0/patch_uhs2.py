# Patches the vendored USB-Host-Shield-20 library for the Polaris board.
# Re-applied (idempotently) before every build via extra_scripts, so the fixes
# survive `pio pkg update` / a wiped .pio/libdeps.
#
# INT pin: the library hardcodes the ESP32 MAX3421E interrupt as GPIO17
# (MAX3421e<P5,P17>), but GPIO17 is the SD card's MISO line here and the
# MAX3421E INT was moved to GPIO2 in a hardware revision. Point it at P2.
#
# NOTE: an earlier revision also patched USB_XFER_TIMEOUT and the hrTOGERR
# retry loop here to bound USB stalls. Those were reverted - they run on the
# enumeration path too and broke cold-boot device detection. Do NOT reintroduce
# library-wide transfer changes; handle MIDI lockups in MidiUSB instead.
Import("env")
import os

lib_dir = os.path.join(env.subst("$PROJECT_LIBDEPS_DIR"),
                       env.subst("$PIOENV"), "USB-Host-Shield-20")


def patch(filename, old, new, tag):
    path = os.path.join(lib_dir, filename)
    if not os.path.isfile(path):
        print("patch_uhs2: %s not found (skipped)" % filename)
        return
    with open(path, "r", encoding="utf-8", errors="ignore") as f:
        text = f.read()
    if new in text:
        return  # already patched
    if old not in text:
        print("patch_uhs2: anchor not found in %s (library changed?)" % filename)
        return
    with open(path, "w", encoding="utf-8") as f:
        f.write(text.replace(old, new))
    print("patch_uhs2: " + tag)


# 1. Define a P2 pin class in the ESP32 pin block
patch("avrpins.h",
      "MAKE_PIN(P5, 5); // SS\nMAKE_PIN(P17, 17); // INT",
      "MAKE_PIN(P5, 5); // SS\nMAKE_PIN(P2, 2); // MAX3421E INT (Polaris hw rev; see patch_uhs2.py)\nMAKE_PIN(P17, 17); // INT",
      "added P2 to avrpins.h ESP32 block")

# 2. Point the ESP32 MAX3421E INT at GPIO2 (GPIO17 is the SD MISO here)
patch("UsbCore.h",
      "typedef MAX3421e<P5, P17> MAX3421E; // ESP32 boards",
      "typedef MAX3421e<P5, P2> MAX3421E; // ESP32 (Polaris: INT=GPIO2, GPIO17=SD MISO; see patch_uhs2.py)",
      "set ESP32 MAX3421E INT to P2")
