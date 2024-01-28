# About

I'm committing the initial AI-written version of this project to illustrate how useful AI can be, and also how you have to be careful not to trust it too much.

In 1 minute, I was able to generate the boilerplate C code for subscribing to an MQTT topic. I'm likely going to end up re-writing most of it as I shape the code into what I want, but this lets me hit the ground running. IMHO, It's very important for getting side projects off the ground. The C code is 100% generated via phind.com's AI. I only changed the subscription topic name in this initial commit. I wrote the Makefile, .envrc, default.nix, and README.md by hand. The LICENSE file comes from github.

## Prompt

I used the following prompt on the Phind v9 model:

> Write a C program that will subscribe to an MQTT topic named "zigbee2mqtt/devices/light1" and print messages to stdout.

In case phind.com links are persistent, here's the link to my query:
https://www.phind.com/search?cache=jm4tvxlbxu07xszk5d7vk3v1

## Bugs

As generated, I found one single bug. I'm keeping it here, but I'll fix it in the next commit. Can you find it?

Hint: You may need to read the documentation of the paho-mqtt client library.
