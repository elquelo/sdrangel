<h1>Jogdial Controller Feature Plugin</h1>

<h2>Introduction</h2>

This plugin aims at supporting frequency control via a "jog dial". A jog dial, jog wheel, shuttle dial, or shuttle wheel is a type of knob, ring, wheel, or dial which allows the user to shuttle or jog through audio or video media.

It is designed to support the Contour ShuttleXpress and ShuttlePro products but as it is keyboard keys based any device capable of reproducing the same key sequences (including a standard keyboard - see next) can interact with this plugin.

In the last section you will find details about the key sequences and placement of supported knobs on the ShuttleXpress and ShuttlePro.

Note that it is based on Qt keyboard events. These events are supported only for GUI applications therefore this plugin is not built in the server variant.

<h2>Interface</h2>

![Jogdial controller GUI](../../../doc/img/JogdialController_plugin.png)

<h3>1: Start/Stop</h3>

Use this toggle to activate or deactivate the plugin. Note that for the control to be effective the plugin should also have the focus. You can check the focus state with the focus indicator (2)

<h3>2: Focus indicator</h3>

This indicator turns green if the plugin has focus. To set the focus on the plugin just click anywhere on it. The controls are effective only if the plugin has focus.

The refresh button (3) and the channel selection combo box (4) do not take the focus when clicked: using them does not interrupt the control. Clicking on any other part of SDRangel (spectrum, other windows) moves the focus away. To get it back without the mouse press Shift+J (map it to a button of your choice, for example K9): it works whichever window of SDRangel has the focus, as long as the plugin is started.

<h3>3: Refresh channels list</h3>

Use this button to refresh the list of available channels. It will scan through all the device sets presents in the instance to list their channels in the combo box next (4).

Note that on first start the list in (4) is empty therefore you must press this button right after the plugin is started with (1)

<h3>4: Select channel</h3>

Use this combo box to select which channel to control. The list item is formatted this way:

  - R or T for a source (Rx) or sink (Tx) device set
  - The sequence number of the device set
  - The sequence number of the channel after the semicolon separator
  - The type of channel

The frequency of the device of the device set the channel belongs to can also be controlled when device control is selected. The type of control is displayed in (5)

To select the type of control:

  - Press K5 on the Contour device or ¨D" (shift+D) on the keyboard for device control
  - Press K9 on the Contour device or ¨C" (shift+C) on the keyboard for channel control
  - Press "S" (shift+S) on the keyboard for spectrum control (see the section on spectrum control)
  - Press "M" (shift+M) on the keyboard to go to the next type of control: device, channel, spectrum and back to device. This lets you cycle through the three types with a single button: map it to the button of your choice on the Contour device, for example K5

<h3>5: Control type indicator</h3>

It displays "D" when in device control, "C" when in channel control or "S" when in spectrum control mode.

<h3>6: Motion parameters</h3>

The "Motion" section sets how the jog wheel and the shuttle ring behave, separately for each control type (device, channel, spectrum). The combo box selects the type edited; it follows the control type in use. Changes apply at once and are saved with the plugin settings. "Defaults" restores the values of the selected type. The sliders do not take the keyboard focus so using them does not interrupt the control.

  - **Speed 1**: speed at ring position 1 (kHz/s for a device, Hz/s for a channel, percent of the displayed window per second for the spectrum)
  - **Ratio**: speed factor between two successive ring positions. The line below the sliders shows the resulting speed of positions 1 to 7
  - **Accel**: time in ms the speed takes to follow the ring when speeding up. Lower is quicker
  - **Brake**: same when slowing down or stopping
  - **Jog from**: number of jog clicks per second above which the jog accelerates
  - **Jog max**: maximum acceleration of the jog
  - **Zoom**: smoothing time of the zoom animation in ms (spectrum only)
  - **Retune**: minimum delay in ms between two retunings of the device (device only)

<h3>Spectrum control</h3>

In spectrum control (S) the jog wheel and the shuttle wheel act on the frequency zoom of the spectrum and waterfall display instead of changing a frequency. The display controlled is the one of the device set the selected channel (4) belongs to. The frequency of the device or of the channel is not modified in this mode.

  - The **jog wheel** zooms in (right) or out (left) keeping the frequency of the selected channel in place on the display. If no channel is selected it zooms around the center of the displayed window. The Ctl and Shift keys select the zoom ratio of each click: no key: x1.1, Ctl key: x1.25, Shift key: x1.5, Ctl+Shift keys: x2. The jog acceleration applies to the zoom too and the zoom is animated: the display moves progressively towards the requested zoom. The zoom factor is limited between 1 (full span) and 50 as it is with the mouse wheel.
  - The **shuttle wheel** moves the zoomed window towards lower frequencies (left positions) or higher frequencies (right positions) for as long as the position is maintained. The speed follows the position of the ring progressively: it takes a fraction of a second to reach the speed of a new position and to stop when the ring is released. The further you go from the center the faster it goes. The speed is multiplied by about 4.64 for channels and by about 3.16 (half a decade) for devices from one position to the next as you move away from the center. Thus to summarize with the default parameters (all of them can be changed in the Motion section):

  - Center: rest position stops moving
  - &plusmn; 1 step: &plusmn; 30 Hz/s for channels, &plusmn; 3 kHz/s for devices
  - &plusmn; 2 steps: &plusmn; 140 Hz/s for channels, &plusmn; 9.5 kHz/s for devices
  - &plusmn; 3 steps: &plusmn; 650 Hz/s for channels, &plusmn; 30 kHz/s for devices
  - &plusmn; 4 steps: &plusmn; 3 kHz/s for channels, &plusmn; 95 kHz/s for devices
  - &plusmn; 5 steps: &plusmn; 14 kHz/s for channels, &plusmn; 300 kHz/s for devices
  - &plusmn; 6 steps: &plusmn; 65 kHz/s for channels, &plusmn; 950 kHz/s for devices
  - &plusmn; 7 steps: &plusmn; 300 kHz/s for channels, &plusmn; 3 MHz/s for devices

The **keys** are mapped as follows:

  - K5: Select device control
  - K6: Move down through the list of available channels
  - K7: Reset the channel frequency (set to 0) - useful when you loose the channel out of the baseband window
  - K8: Move up through the list of available channels
  - K9: Select channel control

<h3>Mapping to keyboard and keyboard control</h3>

The contour devices proceed by mapping their events to keyboard events and this makes them very adaptable. The Jogdial Controller feature is keyboard event based so you may as well use your keyboard for control. In that case for better visual mapping it is recommended to use a US or US International keyboard.

Contour provides software to perform the mapping on Windows. You just need to install the driver software downloaded from Contour web site, open the support program and then follow instructions. You will have to specify the target program when you create the profile for SDRangel. This is `sdrangel.exe` located in `C:\Program Files\SDRangel`. Note also that the buttons are named differently but the program shows their location on an interactive diagram so this should be no problem to identify them correctly.

When running on Linux you may use [ShuttlePRO](https://github.com/nanosyzygy/ShuttlePRO) for keyboard mapping. It will work also for the ShuttleXpress with minor changes. You will have to identify the path of the Shuttle device and run the program with the device path as argument in a terminal. See the last section for details.

<h4>Keyboard mapping</h4>

The shuttle devices controls are mapped according to the following table. K5 to K9 represent the shuttle keys. S-7 to S7 represent the shuttle (outer ring) positions S0 being the rest middle position. JL is a jog step to the left and JR a jog step to the right:

<table>
  <tr>
    <th>Shuttle</th>
    <th>Keyboard</th>
    <th>Key sequence (US)</th>
    <th>Action</th>
  </tr>
  <tr>
    <td>K5</td>
    <td>D</td>
    <td>Shift+D</td>
    <td>Device control</td>
  <tr>
  <tr>
    <td>K6</td>
    <td>&larr;</td>
    <td>&larr;</td>
    <td>Select previous channel in list</td>
  <tr>
  <tr>
    <td>K7</td>
    <td>Home</td>
    <td>Home</td>
    <td>Reset channel frequency (set to 0)</td>
  <tr>
  <tr>
    <td>K8</td>
    <td>&rarr;</td>
    <td>&rarr;</td>
    <td>Select next channel in list</td>
  <tr>
  <tr>
    <td>K9</td>
    <td>C</td>
    <td>Shift+C</td>
    <td>Channel control</td>
  <tr>
  <tr>
    <td>-</td>
    <td>S</td>
    <td>Shift+S</td>
    <td>Spectrum control</td>
  <tr>
  <tr>
    <td>-</td>
    <td>J</td>
    <td>Shift+J</td>
    <td>Set the focus on the plugin from anywhere in SDRangel (map it to a button of your choice, for example K9)</td>
  <tr>
  <tr>
    <td>-</td>
    <td>M</td>
    <td>Shift+M</td>
    <td>Next control type: device, channel, spectrum (map it to a button of your choice, for example K5)</td>
  <tr>
  <tr>
    <td>S-7</td>
    <td>&amp;</td>
    <td>Shift+7</td>
    <td>Continuous -1 MHz / -1 GHz move</td>
  <tr>
  <tr>
    <td>S-6</td>
    <td>&Hat;</td>
    <td>Shift+6</td>
    <td>Continuous -100 kHz / -100 MHz move</td>
  <tr>
  <tr>
    <td>S-5</td>
    <td>&percnt;</td>
    <td>Shift+5</td>
    <td>Continuous -10 kHz / -10 MHz move</td>
  <tr>
  <tr>
    <td>S-4</td>
    <td>&dollar;</td>
    <td>Shift+4</td>
    <td>Continuous -1 kHz / -1 MHz move</td>
  <tr>
  <tr>
    <td>S-3</td>
    <td>#</td>
    <td>Shift+3</td>
    <td>Continuous -100 Hz / -100 kHz move</td>
  <tr>
  <tr>
    <td>S-2</td>
    <td>&commat;</td>
    <td>Shift+2</td>
    <td>Continuous -10 Hz / -10 kHz move</td>
  <tr>
  <tr>
    <td>S-1</td>
    <td>&excl;</td>
    <td>Shift+1</td>
    <td>Continuous -1 Hz / -1 kHz move</td>
  <tr>
  <tr>
    <td>S0</td>
    <td>0</td>
    <td>0</td>
    <td>Stop continuous move</td>
  <tr>
  <tr>
    <td>S1</td>
    <td>1</td>
    <td>1</td>
    <td>Continuous +1 Hz / +1 kHz move</td>
  <tr>
  <tr>
    <td>S2</td>
    <td>2</td>
    <td>2</td>
    <td>Continuous +10 Hz / +10 kHz move</td>
  <tr>
  <tr>
    <td>S3</td>
    <td>3</td>
    <td>3</td>
    <td>Continuous +100 Hz / +100 kHz move</td>
  <tr>
  <tr>
    <td>S4</td>
    <td>4</td>
    <td>4</td>
    <td>Continuous +1 kHz / +1 MHz move</td>
  <tr>
  <tr>
    <td>S5</td>
    <td>5</td>
    <td>5</td>
    <td>Continuous +10 kHz / +10 MHz move</td>
  <tr>
  <tr>
    <td>S6</td>
    <td>6</td>
    <td>6</td>
    <td>Continuous +100 kHz / +100 MHz move</td>
  <tr>
  <tr>
    <td>S7</td>
    <td>7</td>
    <td>7</td>
    <td>Continuous +1 MHz / +1 GHz move</td>
<tr>
  <tr>
    <td>JL</td>
    <td>&darr;</td>
    <td>&darr;</td>
    <td>Single -1/-10/-100/-1000 (Hz/kHz) move</td>
  <tr>
  <tr>
    <td>JR</td>
    <td>&uarr;</td>
    <td>&uarr;</td>
    <td>Single +1/+10/+100/+1000 (Hz/kHz) move</td>
  <tr>
</table>

<h4>US keyboard mapping</h4>

![US keyboard layout](../../../doc/img/USKeyboardLayout.png)

<h3>Running the Contour devices in Linux</h3>

As briefly introduced earlier you may use [ShuttlePRO](https://github.com/nanosyzygy/ShuttlePRO) for keyboard mapping.

The program will normally work as-is for ShuttlePRO. It will also work for ShuttleXpress with a few adaptations detailed next.

Firstly you need to add the proper udev rules that are slightly different. In fact you just need to change the "ATTR" line by:
```
ATTRS{name}=="Contour Design ShuttleXpress" MODE="0644"
```
Secondly the device path is different for ShuttleXPress. You have to look for your device in the path starting with `/dev/input/by-id/usb-Contour_Design_` In most cases this will be: `/dev/input/by-id/usb-Contour_Design_ShuttleXpress-event-if00`

You will then use this path as argument to the `shuttlepro` program:

`shuttlepro /dev/input/by-id/usb-Contour_Design_ShuttleXpress-event-if00`

You may change the argument in the `shuttle` script for convenience:

`exec shuttlepro /dev/input/by-id/usb-Contour_Design_ShuttleXpress-event-if00`

In any case you need to specify the mapping in the `~/.shuttlerc` file by adding a section for SDRangel like this:
```
[SDRangel]
 K5 "D"
 K6 XK_Left
 K7 XK_Home
 K8 XK_Right
 K9 "C"

 S-7 XK_Shift_L/D "7"
 S-6 XK_Shift_L/D "6"
 S-5 XK_Shift_L/D "5"
 S-4 XK_Shift_L/D "4"
 S-3 XK_Shift_L/D "3"
 S-2 XK_Shift_L/D "2"
 S-1 XK_Shift_L/D "1"
 S0 "0"
 S1 "1"
 S2 "2"
 S3 "3"
 S4 "4"
 S5 "5"
 S6 "6"
 S7 "7"

 JL XK_Down
 JR XK_Up
```

