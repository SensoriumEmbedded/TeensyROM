
set LocalPath=C:\Users\trav\AppData\Local
set LocalT4CorePath=%LocalPath%\Arduino15\packages\teensy\hardware\avr\0.60.4\cores\teensy4

:: delete temp local cache:
rmdir /s /q "%LocalPath%\Temp\arduino\cores"
rmdir /s /q "%LocalPath%\Temp\arduino\sketches"
