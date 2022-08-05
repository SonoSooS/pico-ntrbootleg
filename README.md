# [REDACTED]

> Note: The source code presented here is the public version of this project, modified to comply with Github's Terms of Service. While the public and private versions are functionally equal, the source code between them is not.


After lots of optimization effort to push the RP2040's unique features to their absolute limit, it's finally possible to perform an ntrboot boot using this tool.


## Credits

- profi200 for giving helpful tips and optimization ideas
- gbatek for the enPng/KEY2 crypto

## Usage

> TODO: currently only [minfirm](https://github.com/aspargas2/minfirm) works with the public version, but some issues still need to be solved in the private version before it can be published

> TODO: the Pico has to be unplugged and re-plugged after a successful and/or an unsuccessful boot because reset detection is currently broken in the private version (need a new Pico before I can investigate why)

Hook up the pins to the cartridge adapter like this:
- GP2 - GP9: DAT0 - DAT7
- GND: GND
- GP10: /CLK
- GP11: /CS (or /CS1, for ROM select)
- GP12 (unused): /CS2 (SPI flash select)
- GP13 (currently unused, but needed for a later patch): /RST

> Warning: DO NOT HOOK UP CARTRIDGE 3v3 TO THE PICO WITH USB CONNECTED!

You need an USB power source (no data needed after programming) before turning the 3DS on, because the boot time of the Pico is way too slow to be able to use it standalone, even though the 3DS has no problem powering the Pico from the cartridge port alone.


## Building

Before building, set your binary flavor in `CMakeLists.txt`:
- for in-flash binary (should be default), uncomment the lines with `neg.ld` and `blocked_ram`, and comment out the lines with `neg_ram.ld` and `no_flash`
- for in-RAM binary, it's a bit more complicated...
    - uncomment the lines with `neg_ram.ld` and `no_flash`, and comment out the lines with `blocked_ram` and `neg.ld`
    - follow the instructions at [oof2](https://github.com/SonoSooS/oof2) to patch your SDK
    - after successfully building the project, patch the resulting .uf2 to somewhere else using `oof2.py`, then you can drag that binary to the Pico's USB bootloader (unpatched .uf2 will not work, and will just stay there in the OS disk cache)

> Note: On Windows you need to patch an older version of CMake (3.20), so it doesn't try overriding the ARM gcc used for building the source code with MSVC.

> Notice: It's recommended to use the Ninja generator, as every other generator fails (for me) in some ways at compile-time.

You can just build this like every other Pico SDK project. See `genbuild.bat` for an example way to invoke CMake from a `build` folder.

Assuming you used the Ninja generator, just run `ninja -j1` from the `build` folder.

> Note: on Windows you need native MSVC in PATH to build elf2uf2.exe successfully
