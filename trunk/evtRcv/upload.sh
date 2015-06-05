adb push $TOOLCHAIN/arm-linux-androideabi/lib/libgnustl_shared.so /data/local/tmp
adb push $ACE_ROOT/lib/libACE.so.6.3.1 /data/local/tmp
adb push libs/armeabi/libACE.so /data/local/tmp
./upExec.sh
