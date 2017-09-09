HW2 Prototype Documentation
===========================

Overview
--------

This device is like a Nano S, but in a formfactor more suitable for the keychain hodling.

The screen has been removed to make it more like a Nano, and a led added to make it more secure than a Nano.

It features the same BOLOS dual chip architecture as in the Nano S and the Blue. Meaning apps can be loaded onto it. The sole difference is that at boot the loaded application is autorun.

The device enables user to compile and load their own application, HW2 Prototype is binary compatible with the Nano S 1.3.1 SDK.

The button is mapped onto the RIGHT_BUTTON of the Nano S s that Nano S application can be loaded, and consent required by application can be agreed by a click on the sole button. Application can be ported from the Nano S by only adding a LED command mechanism (to perform blinking and stop it when consent is aquired). 

Boot modes
----------

The device, as per the Nano S, has 3 startup mode:
 - normal mode, the MCU boots the SE which boots the last installed application if any ;
 - recovery mode, the MCU boots the SE which stay in the content management application ;
 - bootloader mode, the MCU bootloader is run and wait for a MCU upgrade.

To enter recovery mode, unplug, press the button and plug device, wait for 3 seconds, release when the led blinks rapidly.
To enter bootloader mode, unplug, press the button and plug device, wait 3 seconds, the led blinks rapidly, wait for more 3 seconds, release when the led blinks slowly.

