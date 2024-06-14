# Cuefinger 1.x

Cuefinger 1 gives you the possibility to remote control Universal Audio's
Console Application via Network (TCP).

In a recording studio this gives the studio musician the possibility to access the cue mix via a linux, android or windows tablet.
Multiple connections from different devices at the same time are possible. E.g using an UA Apollo interface with 4 cue busses together with 4 tablet-pcs makes it possible for 4 musicians to each control their individual cue mix.

I use cuefinger in my studio since a few years now. Until now I couldn't find the time to publish it for others to use, but better late than never.
Cuefinger runs in my studio on 3 low budget HP Stream 7 tablets with Linux Mint and I also use an old win10 tablet (32bit). They remotely control my cascaded Apollo Silver and Apollo 16 black. I also tested it with Apollo Solo. There is no iOS Version since I have no experience in writing for this platform.

I hope you find this software useful, feel free to contact me or check out my music.

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
## Screenshots
Here are some screenshots of it's responsive GUI and the select channels feature:
![Screenshot 2024-06-14 182150](https://github.com/franqulator/cuefinger/assets/97669947/7a134bdd-e64b-4a34-be21-a5623b57c3d8)
![Screenshot 2024-06-14 182353](https://github.com/franqulator/cuefinger/assets/97669947/44096b95-a7db-49f5-9842-79d1a767d560)
![Screenshot 2024-06-14 182422](https://github.com/franqulator/cuefinger/assets/97669947/bd7e8f7d-6de4-45bc-b676-a4bcd37e5f90)
![Screenshot 2024-06-14 182529](https://github.com/franqulator/cuefinger/assets/97669947/bb107a5b-4f8d-4206-a351-1f25514516ff)
![Screenshot 2024-06-14 182719](https://github.com/franqulator/cuefinger/assets/97669947/9dbd973a-4ebb-44cc-aa13-be88ca3b502e)
![Screenshot 2024-06-14 182727](https://github.com/franqulator/cuefinger/assets/97669947/e1a6a694-e5a8-4ce5-b9ed-b4535765eee3)
