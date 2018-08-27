# ScorePanel
multiple sport score panel

This is the **"Score Panel"** part of the **"Score Panels System"**.
The **“Score Panel”** software show the score and other related information under control of the **“Score Controller”**.

Each **"Score Panel"** can be individually configurable by the **"Score Controller"** in such a way to show
the score information only or instead the score and, on request, **slides** and **videos** or even **live images**
of the game field.

Here you can see the Score Panel for the _Volley_:

![Volley Score Panel](/images/ScorePanel.png)

The **"Score Panel"** software can be run on Linux computers (tested on UBUNTU 18.04 LTS) or on an Android tablet
or else on a Raspberry Pi3 connected to a monitor, mouse and keyboard.

The **"Score Controller"** and the **"Score Panels"** communicate via Network (either wired or wireless).

At present there are only 3 sports that have the corresponding scoreboard:

* Volley
* Handball
* Basket

but the system can be easily extended to many other sports.

The program is written in C++ by using the powerfull framework Qt. I have installed the version 5.11.1 on my Ubuntu laptop and version 5.7.2 on my Raspberry Pi3.

## Installation
There are already built versions for Android, Ubuntu and Raspberry Pi3 at:

https://github.com/salvato/ScorePanel_executables

For Ubuntu and Raspberry you can simply download the executable in a directory of your choice and the run the App provided that all the needed libraries are presents.

To check if there are missing libraries you can run the following command:

`ldd ./scoreController`

If you want to install the APP for Android, download the `scoreController.apk` file into the uSD card of your tablet/phone,
navigate to the folder where you have downloaded the apk file and double tap on it.
