# Cuefinger 1.01 beta

Cuefinger 1 gives you the possibility to remote control Universal Audio's
Console Application via Network (TCP).

In a recording studio this gives the studio musician the possibility to access the cue mix via a linux or windows tablet.
Multiple connections from different divices at the same time are possible. E.g using an UA Apollo interface with 4 cue busses together with 4 tablet-pcs makes it possible for 4 musicians to each control their individual cue mix.

I use cuefinger my studio since a few years now. Until now I couldn't find the time to publish it for others to use, but better late than never.
Cuefinger runs in my studio on 3 low budget HP Stream 7 tablets with Linux Mint and I also use an old win10 tablet (32bit). They remotely control my cascaded Apollo Silver and Apollo 16 black. I also tested it with Apollo Solo. There is no Android-/iOS Version since I have no experience in writing for these platforms.

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

### To run Cuefinger on Linux you might have to install the SDL2 runtimes:
- Open Terminal
> sudo apt-get install libsdl2-2.0<br>
> sudo apt-get install libsdl-ttf2.0-0

---

### How to compile on Windows 10/11:
You need SDL2 developer package (https://github.com/libsdl-org/SDL/releases/tag/release-2.30.0).<br>
You can use the Visual Studio 2022 project files, make sure that you copy the SDL2 include and bin into the project folder or change the include and lib paths in the gfx2d_sdl.h

### How to compile on Linux:
- Open the Terminal in the project folder:
- You might need to install the SDL2 libraries:
>sudo apt-get install libsdl2-dev<br>
>sudo apt-get install libsdl2-ttf-dev
- create a folder named linux where the application will be created to
- run the makefile
>make

