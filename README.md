# Screen Tapper

Late last year, I got addicted to a mobile game containing a currency that can only be acquired by tapping the screen every so often. While I've been running the game 24/7 on an old device, I'm unable to claim this currency because I leave the device at home, and my schedule only allows me to collect for a few hours each day. After getting tired of being bottlenecked by this currency, I decided to try to create a device that would farm for me while I was away. The terms of service of this specific game do explicitly state that the use of external software/hardware to gain an unfair advantage can result in the abuser's account being banned. Because of this, my goal with this device is to be as undetectable as possible, so I'll omit as many details of the specific game as possible. Regardless, I have accepted the fact that I could be banned for this, and I'm okay with that. Learning about electronics and continuing to broaden my engineering skills is well worth the risk, especially since I've been having so much fun with this project.

Overall, this project can be broken up into several main components: the electronic hardware, the software, and the mechanical housing.

## Electronic Hardware

The brains of this device started out on an Arduino Uno R3, but I have since upgraded to an Arduino Mega 2560 for the additional space and I/O support. The power comes from a 5V 6A DC wall plug that interfaces with a barrel jack adapter. Interaction with the mobile device is carried out by two 5V solenoid valves that are controlled using one MOSFET each. A Real Time Clock (RTC) module provides time-keeping, and a series of buttons allow for limited manual control. An LCD screen provides additional external information. The RTC, MOSFETs, buttons, etc. are all connected using a couple of breadboards.

## Software

The software for the arduino is contained in a single .ino file. The RTC is used to automatically simulate a "bed time" for the device to better match the behavior of a real person. During normal sleeping hours, the device ceases operations, and at a defined wake-up time, it resumes. Fortunately, the game resource being collected is available on a somewhat consistent basis. The average length of time between collections was first determined manually. That length of time (plus some small buffer) acts as the basis for the activation timer for the solenoids. Some additional random noise and occasional delays is then added to simulate the randomness of a human playing the game organically.

## Mechanical housing

The current housing for the device is made entirely from cardboard, ductape, and a few zip ties.

## Future Plans

### Electronic Hardware

While I've upgraded from jumper cables to custom lines of #22 solid core wire (which made a surprisingly big improvement in overall quality of life when working with this thing), I eventually want to learn how to design a single, custom PCB to power the device. As a mechanical engineer who always avoided the electronics lab, I'm expecting this to be the most challenging part of the project. My vision is to have a single board that everything is soldered to and has sufficient customization via on-board controls (buttons/switches) that never needs to be plugged into a computer again after the software is initially installed. One nice-to-have feature I'd like to add eventually is the ability to log all actions onto an SD or micro SD card for future analysis to assess the device's performance. Another nice-to-have feature would be the ability to completely shut off the LCD display.

### Software

The software is currently functional, but I want to add functionality to support more manual control via the buttons/switches in the device. Specifically, I want to be able to tune the tap duration of the solenoids. I'd also like to be able to turn the individual solenoids on and off manually and individually. I'd like to be able to override the bedtime. Depending on the complexity of manual inputs available, I may need to update the LCD output to make it easier to tell what settings are active.

### Mechanical Housing

Possibly the biggest quality of life improvement for this project would be an upgraded housing for the device. While charming, the current duct tape and cardboard design is clunky and flimsy. I'd eventually like to 3D print a sleek enclosure. The current plan is to print a version 1.0 to accommodate the electronic hardware I'm using currently. Then, if I'm able to successfully design a custom PCB, I could revise the design and release a final version 2.0 with seamless integration of the custom PCB, including button/switch mounts on the exterior of the housing, a flush cutout for the LCD, and an easily reachable and secure barrel jack plug for the power supply.
