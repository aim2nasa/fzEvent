#!/bin/sh

ndk-build clean
ndk-build
cp obj/local/armeabi/dmt ./bin
