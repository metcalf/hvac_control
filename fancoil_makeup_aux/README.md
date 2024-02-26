Read user row:

```
~/.platformio/packages/tool-avrdude/avrdude \
  -C /Users/andrew/.platformio/packages/tool-avrdude/avrdude.conf \
  -p attiny826 -P /dev/cu.usbserial* -b 115200 -c serialupdi \
  -U userrow:r:-:h -x rtsdtr=high
```

Write user row:

```
USER_ROW_BIN=cfg/secondary_fancoil.bin; \
~/.platformio/packages/tool-avrdude/avrdude \
  -C /Users/andrew/.platformio/packages/tool-avrdude/avrdude.conf \
  -p attiny826 -P /dev/cu.usbserial* -b 115200 -c serialupdi \
  -U userrow:w:$USER_ROW_BIN:r -x rtsdtr=high
```
