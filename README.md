# DreamControl
**Digital Audio Workstation (DAW) and Studio Monitor Control Surface**

![DreamControl Front Panel](https://i.imgur.com/ogYC02I.png)

**WORK IN PROGRESS! This README will be updated as I move ahead with the project. More detailed info (build photos etc) will go onto a blog at some point.**

This is an open-source project for my 'dream' DAW controller. There are several controllers and plugins on the market that do similar things - I wanted everything neatly integrated, so I came up with this design. The designs and code are free for personal use (not for commercial use).

The system consists of a **hardware control surface** connected via MIDI-over-USB and built on the excellent [MIDIbox](http://www.ucapps.de/) platform, and a **VST plugin** that handles audio processing, and works with Cubase's 'Control Room'.

Currently, the system is designed for my DAW of choice which is **Cubase**, and the plugin can be used with it's Control Room feature, but it could easily be adapted to support Logic, Bitwig, or other DAWs.

## Features
 - Switch between your mix and 6 other sources
 - Switch between 4 sets of monitor speakers
 - Monitor level knob (ALPS endless encoder) with 16x RGB LED ring to show position. 48dB range in 1dB steps (3 brightness levels per LED)
 - No analog electronics required - converters can be connected directly to monitors for cleanest signal path
 - Monitor level attentuation in plugin, or using external MIDI-controlled device, e.g. RME TotalMix.
 - Monitor Mute, Dim and Reference options (dim and reference levels adjustable in plugin)
 - Mono mode, Side Band Solo, and 'Loud' mode (an inverted Fletcher-Munson EQ curve that sounds a bit like Yamaha NS10M monitors)
 - Low, Low-Mid, High-Mid and High frequency band solo (crossover points adjustable in plugin)
 - Channel strip controls: 100mm motorised ALPS fader, Mute, Solo, Automation Read and Write
 - Talk Back (for Cubase Control Room)
 - Standard transport controls: Play, Stop, Click, Record, Return, Loop, Backwards, Forwards. These can be assigned using the plugin, e.g. to DAW macros
 - Three 15x RGB LED level meters, that can show True Peak or LUFS (EBU R128 / ITU 1770 standard), or a combination (e.g. True Peak + LUFS Momentary)
 - Two different True Peak dB scales, two different LUFS scales (absolute and relative)
 - 128x64 pixel OLED display, showing various meter values: LUFS Integrated in large numbers, and smaller numbers showing True Peak Max L/R, LUFS Momentary Max, LUFS Target (can be set using plugin)
- Monitor Level Knob can also be set to 'MIDI modulator mode' by pressing VOL/MOD, so instead of changing monitor level it can output MIDI CC messages
## Background
When writing, mixing or mastering, I often switch between what I'm working on and reference material, and switch between several sets of speakers. I also often want to hear the mix in mono, hear the side band solo'd, and also individual frequency bands. I also want easily readable True Peak and LUFS meters, and a physical motorised fader and related channel strip and transport controls.

And I want these controls to be tactile, physical controls. And I don't want to have to look at a touchscreen or regular display to do any of this.

So the DreamControl was born!

I had been wanting to make something using the MIDIbox platform for a while, and this was a good excuse. The hardware build itself is a little complex, but could be optimised considerably, and in future I may look at designing a custom PCB. For the prototype, I used 'off the shelf' PCBs from the MIDIbox project, which were quick to put together and worked well. It only just fitted inside the enclosure I had chosen, again in future this could be optimised.

Another inspiration for the project was finding out that the high-end, expensive EAO brand clicky switches that I love (used for example on SSL consoles), are now also Made in China for under £1!
## Hardware
The hardware consists of
 - STM32F407 Cortex-M4 microcontroller, on [low cost evaluation board
](https://www.st.com/en/evaluation-tools/stm32f4discovery.html), connected to PC via MIDI-over-USB.
 - MIDIbox boards:
	 - [MBHP_CORE_STM32F4](http://www.ucapps.de/mbhp_core_stm32f4.html) board
	 - [MF_NG motorfader board](http://www.ucapps.de/mbhp_mf_ng.html), based on PIC18F452
	 - [DOUT board](http://www.ucapps.de/mbhp_dout.html) for button LEDs
	 - [DIN board](http://www.ucapps.de/mbhp_din.html) for buttons
 - 35x Square Illuminated Momentary push-buttons (model [LAZ16-11](https://www.ebay.co.uk/itm/Square-micro-button-LAZ16-11-self-lock-no-lamp-button-switch-3-foot-diameter16mm-/162438920326))
 - Custom display boards:
	 - SSD1306 [OLED display](https://www.amazon.co.uk/SSD1306-128x64-Display-Module-Electronics/dp/B00KCM9JMG) (128x64 pixels, SPI interface)
	 - WS2812 RGB LED strip, cut to 15 LED sections for meters ([NeoPixel brand](https://www.adafruit.com/product/1507), 144 LED per metre strip)
	 - WS2812 RGB LED ring ([NeoPixel brand](https://www.adafruit.com/product/1463))
 - ALPS 100mm motorised fader ([RSA0N11M9A0K](https://www.reichelt.com/de/en/alps-rsa0n-studio-fader-motor-and-touch-sense-10k-rsa0n11m9-lin10k-p73884.html?ARTICLE=73884&&r=1))
 - 5V 1.5A / 12V 1.5A power supply (butchered from an old external hard drive enclosure)
 - [Aluminium desktop enclosure](https://www.reichelt.com/de/en/aluminium-desktop-enclosure-181-2-x-68-2-x-250-mm-atph-1865-250-p126238.html?), by BOPLA.
 - Front panel custom CNC milled and engraved by [Schaeffer](https://www.schaeffer-ag.de/en/), designed with their [front panel designer app](https://www.schaeffer-ag.de/en/downloads/front_panel_designer/).
 
The FPD (Front Panel Designer) file can be found in the `/frontpanel` directory. A Photoshop file for the button labels is also present (TODO). These push buttons can easily have labels inserted into their caps - so I simply printed out labels onto acetate transparency. This is a good system because it means features can be added at a later date simply by changing the labels, without needing to make changes to the front panel.

The motor fader calibration SysEx file (for use with MIOS Studio) can be found in the `/calibration` directory. This is the calibration I am using for the ALPS fader above, although I'm not exactly sure how this is supposed to work, so will probably be revised over time!
 
TODO: Schematics and hardware build photos

## Firmware
The DreamControl firmware is written in C and uses the excellent MIDIBox operating system, which is called MIOS32 and runs on the STM32F407. It is based on a Real Time Operating System derived from FreeRTOS. The toolchain for MIOS32 is based on GCC.

To build and flash the firmware, you'll need to obtain:

 - The GCC toolchain. I'm on Windows so I use [MSYS](http://www.mingw.org/wiki/MSYS).
 - [MIOS32 Bootloader](http://www.ucapps.de/mios32_download.html), which allows you to quickly flash new firmware over USB MIDI.
 - [ST LINK utility](https://www.st.com/en/development-tools/st-link-v2.html) from ST Microelectronics, which allows you to flash the MIOS32 bootloader into a virgin STM32F4 (only needed once).
 - MIOS32 from the [SVN repository](http://svnmios.midibox.org/listing.php?repname=svn.mios32&path=/trunk/) 
 - [MIOS Studio](http://www.ucapps.de/mios_studio.html) which is a Windows/Mac app for flashing new firmware, viewing MIDI data, and sending and receiving debugging information via MIDI, as well as calibrating the motorfader (MF) board.

The code is split into various modules. Code files and function names begin with the prefix `DC_`, except MIOS32 hooks which begin `APP_`.

## Plugin
The audio side of the system is handled by a [VST](https://en.wikipedia.org/wiki/Virtual_Studio_Technology) plugin running in the DAW - in our case, in Cubase's 'Control Room' insert section, but could equally be on the master bus. The plugin is written in C++ and uses the excellent JUCE framework.
 
To build the plugin, you'll need to:

 - Download the [JUCE framework](https://shop.juce.com/get-juce). It is free for personal use.
 - Make sure the `JuceLibraryCode` folder is copied to the `/plugin` folder.
 - Use Visual Studio to build the plugin.
 
The plugin handles various audio processing and metering tasks:

 - 4-band solo, using Linkwitz-Riley IIR crossover filters
 - MS solo (mono and side band)
 - 'Loud' mode, using 7 IIR peak filters
 - Monitor level, mute, dim and reference levels
 - EBU R128 / ITU 1770 metering - LUFS and True Peak realtime values and max, sent to hardware as MIDI SysEx packet.
 - Various meter options.
 
Some of the controls are mapped directly to the DAW, instead of going through the plugin, such as the fader and channel strip controls, transport controls, and for Cubase, the monitor source and speaker select (handled by Cubase's Control Room). For a DAW without the Control Room feature, this routing would need to be done somehow - Apple Logic has the 'environment' which could facilitate this.

TODO: Make channel strip controls Mackie HUI/Control compliant.

## Future
In the future, I hope to expand the system to deal with 5.1 surround sources. This includes downmix options and L/C/R/Lfe/LS/RS solo buttons, possibly using the top-right band solo and EXT1/EXT2 buttons for this purpose, either shifted, or dedicated.
   
 # 
Copyright © 2018 David Evans. Licensed for personal non-commercial use only. 
<!--stackedit_data:
eyJoaXN0b3J5IjpbMTk1MDkwOTc5LDUxMzUzMTM5NSwtNDkzNz
k0NjExLDYxMjA5MTk2NiwxNzQ1MDMwMTI4LDEyMjM4OTk0OTgs
MTE5NDY0MTc1OSwzNDYyOTE0NywtMTc4OTg0ODAzNywzNTc1MT
c3MDYsLTE1MjAwNjM3XX0=
-->
