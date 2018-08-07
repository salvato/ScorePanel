/*
 *
Copyright (C) 2016  Gabriele Salvato

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/
#include "myapplication.h"
#include <QDebug>

/*!
 * \mainpage The Score Panels
 * \section intro Introduction
 *
 * This is the Client-Side of the "Score Panels System" aimed to replace
 * the very basics score displays adopted in many sport halls.
 *
 * You can have many clients running and they will be all handled by
 * the same Server.
 *
 * It realize a single Score Panel client that can be run, at present,
 * from Linux (tested on UBUNTU 18.04), Raspbian (tested on Raspberry
 * Pi3B with Raspbian) or Android.
 *
 * It does not require any configuration apart to guarantee that the
 * computer is connected to the same network of the "Panel Server".
 *
 *
 * \section install Installation
 * For Ubuntu or Raspberry you have to simply copy the executable(s)
 * from the following url:
 * <a href="https://github.com/salvato/ScorePanel_Executables"> link to the Apps</a>
 *
 * For the Raspberry, in order to be able to use the Slide Show, you have also to
 * copy, in the same folder containing the Score Panel executable, the "SlideShow" App.
 *
 * The system, on Raspberry, make use of the already installed program
 * "omxplayer" to show short movies while on UBUNTU it requires to install "vlc".
 *
 * <pre>sudo apt install vlc</pre>
 *
 * The program depends on some qt5 libraries that can be already installed and on some other that can be missing.
 * To check which libraries are missing you may issue the following command:
 *
 * <pre>ldd ./panelChooser</pre>
 *
 * In my Raspbian version (stretch) two of the needed libraries are missing:
 *
 * <pre>libQt5WebSockets.so.5 => not found</pre>
 *
 * <pre>libQt5SerialPort.so.5 => not found</pre>
 *
 * You can install such libraries by issuing the following command:
 *
 * <pre>sudo apt install libqt5websockets5 libqt5serialport5</pre>
 *
 * \section boh?
 * \subsection sub1 A subsection: How to
 *
 */
int
main(int argc, char *argv[]) {
    MyApplication a(argc, argv);
    int result = a.exec();
    return result;
}
