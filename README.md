# Plugins

A mishmash of sdk experiments, mostly as code examples/exercises rather than useful plugins, but use however you see fit!

This repo does not include the modo SDK library.  To build, you'll need to download it from the foundry website or perhaps some other host (like PixelFondue) in the future.
To Build:
    - Copy the contents of the modo sdk's include/lxsdk directory into modo/include/lxsdk.
    - Copy the contents of the modo sdk's common directory into modo/src/lxsdk
    - CD to the modo directory
    - Run cmake --preset Debug
    - Run cmake --build -t install --preset Debug
You should end up with a kit for each plugin.
