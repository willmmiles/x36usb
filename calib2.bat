@echo off
query -a
echo Place the stick in the full left position.
pause
query -l 1 1
query -v 1 1
echo Place the stick in the full right position.
pause
query -l 1 3
query -v 1 3
echo Place the stick in the full forward position.
pause
query -l 2 1
query -v 2 1
echo Place the stick in the full back position.
pause
query -l 2 3
query -v 2 3
echo Place the stick in center.
pause
query -l 1 2
query -v 1 2
query -v 2 2
query -l 2 2
echo Place the throttle at the top.
pause
query -l 3 1
query -v 3 1
echo Place the throttle at the bottom.
pause
query -l 3 3
query -v 3 3
echo Place the rudder at full left.
pause
query -l 4 1
query -v 4 1
echo Place the rudder at full right.
pause
query -l 4 3
query -v 4 3
echo Place the rudder in the middle.
pause
query -l 4 2
query -v 4 2
echo Place rotary 1 at full left.
pause
query -l 5 1
query -v 5 1
echo Place rotary 1 at full right.
pause
query -l 5 3
query -v 5 3
echo Place rotary 1 in the middle.
pause
query -l 5 2
query -v 5 2
echo Place rotary 2 at full left.
pause
query -l 6 1
query -v 6 1
echo Place rotary 2 at full right.
pause
query -l 6 3
query -v 6 3
echo Place rotary 2 in the middle.
pause
query -l 6 2
query -v 6 2
echo Calibration complete!


