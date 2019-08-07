VecxEMU
========

This is a somewhat revamped port of Vecx, a C programmed Vectrex emulator,
for the Bittboy/RS-97/GCW0. (I hope i can support the Retrostone as well)

Overlays are only partially supported for now.

Requirements
------------
* `libsdl`
* `sdl_image`

Usage
-----

Make sure to have Minestorm (8k rom) named "rom.dat" in $HOME/.vecxemu !
Then you can load any cartridges with ./vecx.elf myrom.bin.

If you use it on the RS-97/Bittboy, then you should make a link entry with GmenuNx
and load a rom with it.

Authors
-------

- Gameblabla - Menu/Frontend (inspired by my menu code from SMS Plus GX), minor changes to core code
* Valavan Manohararajah - original author
* [John Hawthorn](https://twitter.com/jhawthorn) - SDL port
* [Nikita Zimin](https://twitter.com/nzeemin) - audio


