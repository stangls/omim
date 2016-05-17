#!/bin/bash

adb logcat -d | ../../android-ndk-r10e/ndk-stack -sym ./android/obj/local/x86/

