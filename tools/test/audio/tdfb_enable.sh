#!/bin/bash

amixer -c0 cset name='Analog Playback TDFB track' off
amixer -c0 cset name='Analog Playback TDFB angle set' 90
amixer -c0 cset name='Analog Playback TDFB beam' on
