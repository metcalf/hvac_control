Read user row:

```
avrdude -C /Users/andrew/.platformio/packages/tool-avrdude/avrdude.conf \
 -p attiny826 -P /dev/cu.usbserial* -b 115200 -c serialupdi -U flash:w:.pio/build/ATtiny826/firmware.hex:i
```
