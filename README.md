# DoomGeneric NTDrv
This ports DoomGeneric NTNative to kernel-mode driver environment.

# Requirements for building DoomGeneric NTDrv
1. Windows 7 DDK.

# Requirements for running
Only tested on Windows XP 32-bit. I don't know about later versions.

# Building
From the x86/x64 Free Build Environment, cd to the directory where you have cloned this repository, and type 'build' to build the driver. You will find the doomgeneric_ntdrv.sys file in the objfre_wxp_x86 (objfre_win7_x64 if building for x64) folder.

# Installing DoomGeneric NTDrv
Copy it to your system32\Drivers directory of your Windows installation. And then grab the doomgenericntinst.reg from one of the releases and double-click it to install.

# Running
You need my fork of NativeShell to start DoomGeneric NTDrv (bundled with the release). Follow instructions at https://github.com/Cacodemon345/NativeShell to install it.

Type 'doomstart' to start it. It expects the Doom 2 IWAD to reside in C:\Windows\ at the moment. Command line arguments are ignored.

# Bugs:
1. Savegames are broken.
2. Picking a weapon crashes the whole system (bug inherited from original DoomGeneric).
3. It's slow as hell, probably could use FastDoom's EGA drawing code for it.

# License:
Same as original DoomGeneric, except for some files:

i_main_nt.c: ReactOS project license.
doomgeneric_nt.c: Uses code both from ZenWINX and Native Shell (LGPL).
Bundled NDK: Used under the terms of GPLv2.
