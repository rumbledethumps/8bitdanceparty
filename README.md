# 8-Bit Dance Party for the Picocomputer

<!-- rp6502
preset: llvm-mos/Release
publish: danceparty.zip
-->
[![Play 8-Bit Dance Party](https://rumbledethumps.github.io/8bitdanceparty/danceparty/screenshot.png)](https://rumbledethumps.github.io/8bitdanceparty/danceparty/)

[Run it in your browser](https://rumbledethumps.github.io/8bitdanceparty/danceparty/).

Adrian's Digital Basement 8-Bit Dance Party on the
[Picocomputer 6502](https://picocomputer.github.io/). The program has two
scenes. The C64 scene is the Commodore 64 version: a picture of Adrian at
his bench and a three-voice SID tune. The CoCo scene is Paul Fiscarelli's
TRS-80 Color Computer version: a dancer in two poses and a four-voice tune.
Both tunes play on the Picocomputer's OPL2 FM chip.

Enter switches between the C64 and the CoCo. Space turns the disco balls
on and off in the C64 scene and swaps the ADB and ADB ][ headers in the
CoCo scene. On a gamepad, Select or Start is Enter, and A, B, X or Y is
Space.

The project is built from the
[RP6502 project template](https://github.com/picocomputer/rp6502-sdk) and
builds with either compiler, cc65 or llvm-mos. The
[SDK documentation](https://picocomputer.github.io/sdk.html) has the
install steps.

## Reverse Engineering

The C64 version is one program file, and the CoCo version is one binary
file on a disk image. Both were disassembled. The C64 program shows a
multicolor bitmap, moves eight sprites, and plays its tune with the player
routine of GoatTracker 2, a popular C64 music editor. The CoCo program
flips between two screens and plays four voices from wavetables in a loop
timed by counting CPU cycles.

Small emulators, one for the 6502 and one for the 6809, ran both
programs and recorded every screen and every write to the sound hardware.
Those recordings became the reference for the port:

- The pictures became the PNG files in `src`. The CoCo art uses NTSC
  artifact color, so pairs of pixels were decoded into black, white,
  orange and blue.
- The GoatTracker player was rewritten in C. Its output matches the SID
  registers of the original frame for frame.
- The CoCo song became a table of notes and durations.

The C64 version plays its notes about two thirds of a semitone sharp,
because it uses a PAL pitch table on an NTSC machine, and the CoCo version
plays them about a quarter of a semitone sharp. The port plays both at
concert pitch. Two bugs of the C64 version are fixed: the "8-Bit DANCE
PARTY!" title no longer turns black, and the tune no longer starts with a
short stray tone.

## Instrument Selection

The OPL2 makes every sound from pairs of sine-wave operators, so each
instrument is an approximation.

Each CoCo voice uses two OPL2 channels. A search through about 950,000
OPL2 settings found the closest match to the harmonics of each of the
three CoCo wavetables: square, organ and sawtooth. The results are within
about 1 dB on the first ten harmonics.

The C64 tune is translated from the SID registers every frame. Three
designs were built and measured against recordings of both SID chips, the
6581 and the 8580, for pitch, tone, loudness and timing, and the two best
were compared by ear. The winner uses two OPL2 channels for each SID
voice and blends them as the pulse width and the filter change. A short
extra layer adds weight to the kick drum. A last pass matched the loudness,
decay and release of each instrument to the SID recordings.

## Credits

Adrian's Digital Basement 8-Bit Dance Party.

- C64 edition: by Machete, music by Linus.
- CoCo edition: by Paul Fiscarelli.
