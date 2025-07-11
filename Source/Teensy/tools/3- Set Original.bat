
call .\BootLinkerFiles\SetPathDelCache.bat

copy BootLinkerFiles\bootdata.c.orig "%LocalT4CorePath%\bootdata.c"

copy BootLinkerFiles\imxrt1062_t41.ld.orig "%LocalT4CorePath%\imxrt1062_t41.ld"

@echo _
@echo Set up for Original compile to Flash at 0x60000000
@pause
