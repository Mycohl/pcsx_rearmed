#!/bin/sh
adb -s ouya:5555 uninstall net.dancingdemon.pcsx
adb -s ouya:5555 install bin/PCSX-ReARMed-$1.apk
adb -s ouya:5555 shell am start -n net.dancingdemon.pcsx/net.dancingdemon.pcsx.PCSX_ReARMed

