# Cuefinger 1.x

With Cuefinger you can remote control Universal Audio's Console Application via LAN/Wifi.

In a recording studio this gives the studio musician the possibility to access the cue mix via a linux, android or windows tablet/phone. As an example, you could use an UA Apollo interface with 4 cue busses in combination with 4 tablets/phones allowing 4 musicians to control their cue mix individually.
Features:

- Volume, Pan, Mute, Solo, Pre/Post of the Monitor, the CUE- and AUX-Mixes
- Responsive design allows mixing also on tiny screens
- Show only the channels you need via select channels
- Reorder channels
- Mix groups
- Color coding
- Lock to mix (e.g. allow access only to CUE 1)
- Mute All button
- Easily switch between servers
- Low on CPU usage and uses GPU acceleration

I use cuefinger in my studio since a few years now. Until now I couldn't find the time to publish it for others to use, but better late than never.
Cuefinger runs in my studio on 3 low budget HP Stream 7 tablets with Linux Mint and I also use an old win10 tablet (32bit). They remotely control my cascaded Apollo Silver and Apollo 16 black. I also tested it with Apollo Solo. There is no iOS Version since I have no experience in writing for this platform.

I hope you find this software useful, feel free to contact me. You can donate to value countless hours of work by buying <a href="https://franqulator.de" target=_blank>my music</a>.

Cuefinger 1 is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

---

## Credits

Idea & code<br>
Frank Brempel

Graphics<br>
Michael Brempel

Cuefinger uses SDL2 and simdjson<br>
www.libsdl.org<br>
simdjson.org<br>

Thanks to<br>
Antonio Radu Varga whose project UA-Midi-Control helped me to get started<br>
https://github.com/raduvarga/UA-Midi-Control

---

## Donations

If you like and use Cuefinger you can donate by buying one of my songs on <a href="https://franqulator.de">franqulator.de</a> (via <a href="https://franqulator.bandcamp.com">franqulator.bandcamp.com</a>).


---

## Android
### Sideload APK
You can download the apk-file here:
<a href ="https://github.com/franqulator/cuefinger/tree/main/build/android">https://github.com/franqulator/cuefinger/tree/main/build/android</a>

Soon on Google Play store.

---

## Windows 10/11
### Use binary
Just chose the fitting *.exe and run it.

### Compile with Visual Studio 2022
You need SDL2 developer package (https://github.com/libsdl-org/SDL/releases/tag/release-2.30.0).<br>
You can use the Visual Studio 2022 project files, make sure that you copy the SDL2 include and bin into the project folder or change the include and lib paths in the gfx2d_sdl.h

---

## Linux
### Snapstore
<a href="https://snapcraft.io/cuefinger">
  <img alt="Get it from the Snap Store" src="https://snapcraft.io/static/images/badges/en/snap-store-black.svg" />
</a>

### Use binary
The compiled file is target for Ubuntu 22.04 x64. You might have to install the SDL2 runtimes:
- Open Terminal
> sudo apt-get install libsdl2-2.0<br>
> sudo apt-get install libsdl2-ttf-2.0-0
- Make sure it has permission to run as executable
> chmod +x $file

### Compile with makefile
You might need to install the SDL2 developer libraries:
- Open Terminal
>sudo apt-get install libsdl2-dev<br>
>sudo apt-get install libsdl2-ttf-dev
- navigate to the src folder and run make:
> cd src<br>
> make
- you can now run cuefinger
> ../build/linux/cuefinger

---

### Pictures
![vlcsnap-2024-06-27-12h50m30s818](https://github.com/franqulator/cuefinger/assets/97669947/0339aab8-3921-4e12-a602-e648f085fc1d)

![cuefinger_mixer](https://github.com/franqulator/cuefinger/assets/97669947/9da1cd79-44f4-46cb-964b-4421fbd58375)

![cuefinger_mixer_narrow](https://github.com/franqulator/cuefinger/assets/97669947/e87ebb69-b063-4a8e-bf6a-6c4ff87dd4a5)

![cuefinger_select](https://github.com/franqulator/cuefinger/assets/97669947/5115fa2c-a8d6-4e1a-9cb0-71d016cf0544)

![cuefinger_settings](https://github.com/franqulator/cuefinger/assets/97669947/fbeda2a5-7f02-41a3-9b41-019b9477e5ba)
