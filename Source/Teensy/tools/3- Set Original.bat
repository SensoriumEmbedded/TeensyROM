
set LocalPath=C:\Users\trav\AppData\Local

:: delete temp local cache:
rmdir /s /q "%LocalPath%\Temp\arduino\cores"
rmdir /s /q "%LocalPath%\Temp\arduino\sketches"

copy BootLinkerFiles\bootdata.c.orig "%LocalPath%\Arduino15\packages\teensy\hardware\avr\1.59.0\cores\teensy4\bootdata.c"

copy BootLinkerFiles\imxrt1062_t41.ld.orig "%LocalPath%\Arduino15\packages\teensy\hardware\avr\1.59.0\cores\teensy4\imxrt1062_t41.ld"

@echo _
@echo Set up for Original compile to Flash at 0x60000000
@pause
