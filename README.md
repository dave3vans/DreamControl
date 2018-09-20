# DreamControl
**Digital Audio Workstation (DAW) and Studio Monitor Control Surface**

![DreamControl Front Panel](https://i.imgur.com/ogYC02I.png)

This is an open-source project for my 'dream' DAW controller. There are several controllers and plugins on the market that do similar things - I wanted everything neatly integrated, so I came up with this design. The designs and code are free for personal use (not for commercial use).

When writing, mixing and mastering, I often switch between what I'm working on and reference material, and switch between several sets of speakers. I also want to hear the mix in mono, hear the side band solo'd, and also individual frequ

The system consists of a hardware **USB control surface** built using the excellent [Midibox](http://www.ucapps.de/) platform, and a **VST plugin** that sits on your DAW master bus.

Currently, the system is designed for my DAW of choice which is **Cubase**, and the plugin can be used with it's Control Room feature, but it could easily be adapted to support Logic, Bitwig, or other DAWs.

## Features
 - Switch between your mix and 6 different other sources.
 - Switch between 4 different sets of monitor speakers.
 - Monitor level knob (ALPS endless encoder) with 16x RGB LED ring to show position.
 - Monitor Mute, Dim and Reference options (can set dim and reference levels in plugin).
 - Mono mode, Side Band Solo, and 'Loud' mode (an inverted Fletcher-Munson EQ curve that sounds a bit like Yamaha NS10M monitors).
 - Low, Low-Mid, High-Mid and High frequency band solo (crossover points adjustable in plugin).
 - Channel strip controls: 100mm motorised ALPS fader, Mute, Solo, Automation Read and Write.
 - Talk Back (for Cubase Control Room)
 - Standard transport controls: Play, Stop, Click, Record, Return, Loop, Backwards, Forwards. These can be assigned using the plugin, e.g. to DAW macros.
 - Three 15x RGB LED level meters, that can show True Peak or LUFS (EBU R128 / ITU 1770 standard), or a combination (e.g. True Peak + LUFS Momentary).
 - Two different True Peak dB scales, two different LUFS scales (absolute and relative).
 - 128x64 pixel OLED display, showing various meter values - LUFS Integrated in large numbers, and smaller numbers showing True Peak Max L/R, LUFS Momentary Max, LUFS Target (can be set using plugin).
- Monitor Level Knob can also be set to 'MIDI modulator mode' by pressing VOL/MOD, so instead of changing monitor level it can output MIDI CC messages.
<!--stackedit_data:
eyJoaXN0b3J5IjpbLTE1NzY4ODQzMSwzNDYyOTE0NywtMTc4OT
g0ODAzNywzNTc1MTc3MDYsLTE1MjAwNjM3XX0=
-->