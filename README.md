# Screen Tapper

Updated 11/23/25 - current version is 1.2

In 2024, I got addicted to a mobile game containing a currency that can only be acquired by tapping the screen every so often. While I've been running the game 24/7 on an old device, I'm unable to claim this currency because I leave the device at home, and my schedule only allows me to collect for a few hours each day. After getting tired of being bottlenecked by this currency, I decided to try to create a device that would farm for me while I was away. The terms of service of this specific game do explicitly state that the use of external software/hardware to gain an unfair advantage can result in the abuser's account being banned. Arguably, this isn't an unfair advantage because I am not exceeding the limits of what would theoretically be possible if I were to play the game all day every day. Regardless, my goal with this device is to be as undetectable as possible, so I'll omit details about the specific game. I have accepted the fact that I could be banned for this, and I'm okay with that. Learning about electronics and continuing to broaden my engineering skills is well worth the risk, especially since I've been having so much fun with this project.

Overall, this project can be broken up into several main components: the electronic hardware, the software, and the mechanical housing.

## Electronic Hardware

The brains of this device started out on an Arduino Uno R3, but I have since upgraded to an Arduino Mega 2560 for the additional space and I/O support. The power comes from a USB wall adapter through the USB-B port. Interaction with the mobile device is carried out by two 5V solenoid valves that are controlled using one MOSFET each. A Real Time Clock (RTC) module provides time-keeping, and a rotary encoder facilitates user control. An LCD screen provides additional external information. The RTC, MOSFETs, buttons, etc. are all connected using a breadboard.

## Software

The software for the arduino is contained in a single .ino file, but a number of .h/.cpp files organize various functions, such as the LCD display, rotary encoder input, clock. etc. The RTC is used to automatically simulate a "bed time" for the device to better match the behavior of a real person. During normal sleeping hours, the device ceases operations, and at a defined wake-up time, it resumes. Fortunately, the game resource being collected is available on a mostly consistent basis. The average length of time between collections was first determined manually. That length of time (plus some small buffer) acts as the basis for the activation timer for the solenoids. Some additional random noise and occasional delays is then added to simulate the randomness of a human playing the game organically.

## Mechanical housing

The housing has been upgraded using 3d-printed components that securely hold the device and the solenoids. The housing contains internal wire channels for the solenoids to provide a cleaner aesthetic, and the entire thing has been shrunk to be as small as possible with the current overall layout and electronics.

## Future Plans

### Electronic Hardware

While I've upgraded from jumper cables to custom lines of #22 solid core wire (which made a surprisingly big improvement in overall quality of life when working with this thing), I eventually want to learn how to design a single, custom PCB to power the device. As a mechanical engineer who always avoided the electronics lab, I'm expecting this to be the most challenging part of the project. My vision is to have a single board that everything is soldered to and has sufficient customization via on-board controls (buttons/switches) that never needs to be plugged into a computer again after the software is initially installed. One nice-to-have feature I'd like to add eventually is the ability to log all actions onto an SD or micro SD card for future analysis to assess the device's performance. I'd also like to add a third solenoid for even more automated gameplay control.

### Software

Software is mostly complete, offering a variety of manual controls including tap duration, sleep/wake times, bedtime override, and device on/off override. The UI is clean enough, and at this point there are no plans for significant future changes unless I add a third solenoid.

### Mechanical Housing

The current housing is highly simplified, clean, and sturdy. I will need to update it if I add a third solenoid. Otherwise, I would still like to experiment with the idea of creating a travel version, but that would likely only come after I upgraded the electronics to a custom PCB. Otherwise, I may experiment with moving the LCD in order to make the entire device lower profile since that is currently driving the height of the device. That would likely also only come after a custom PCB implementation reduced the wires in the electronics.
