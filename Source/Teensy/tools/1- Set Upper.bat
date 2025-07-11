
call .\BootLinkerFiles\SetPathDelCache.bat

copy BootLinkerFiles\bootdata.c.upper "%LocalT4CorePath%\bootdata.c"

copy BootLinkerFiles\imxrt1062_t41.ld.upper "%LocalT4CorePath%\imxrt1062_t41.ld"

@echo _
@echo Set up for *Upper* compile to Flash
@pause
