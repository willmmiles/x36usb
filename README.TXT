Dhauzimmer's X36/X45 USB filter driver v2.106
README file
02/17/2007
----------------------------------------

Current Features:
-> Calibration of all 6 axes through QUERY.EXE  DO NOT USE WINDOWS CALIBRATION SCREEN!
-> Win98SE/WinME/Win2000/WinXP support
-> USB support only
-> Programmability on buttons, sliders, and hat switches
-> Axis programmability support
-> NO IE INTEGRATION!
-> Generalized mode shifting support.
-> Profiles loadable through included software.

---------------------------------------

Installation:

(1) Uninstall previous Saitek software completely.
(2) Plug in the X36.  Windows should detect it as a USB Human Interface Device.
(3) Go to the device manager and select 'USB Human Interface Device' under HID devices.  Click Properties.  Choose the 'Drivers' tab.
(4) Click the 'Update Driver' button.
--Wizard data from W2K Pro.  Other OS may not be 100% the same.  Follow along as best you can.
(5) Press Next.
(6) Choose 'Display a list of known drivers for this device' and press Next.
(7) Press 'Have Disk'.
(8) Enter the path you extracted the .zip file to.  Folder should contain a .inf and a .sys file.
(9) Choose 'Saitek X36 Flight Controller USB' on the menu and press Next.
(10) Press Next until you complete the installation wizard.  If it prompts you to install an unsigned driver, choose 'yes'.

(11) If you had a previous version of my drivers installed, go to the game controller properties and choose 'reset calibration'.  DO NOT CALIBRATE WITH THE WINDOWS CALIBRATION SCREEN!
(12) Move all six axes of the stick to each extreme and back to the middle.
(13) [Optional] Manually calibrate the stick by running 'calib.bat' or 'calib2.bat' from the driver archive.
(14) [Optional, highly recommended] Download a profile generation tool such as XDProfile (http://xdprofile.freebirdswing.org/) or Biped (http://elex.amu.edu.pl/~mjarek/).

----------------------------------------

Programming:

This driver set uses a binary profile format.  It is highly recommended that you use one of the excellent profile generation tools such as such as XDProfile (http://xdprofile.freebirdswing.org/) or Biped (http://elex.amu.edu.pl/~mjarek/) instead of writing your own binary profiles by hand.

The Query.exe tool is your Swiss Army knife when it comes to using this driver set.  With it you can load a profile or axis profile, clear the loaded profile or axis profile, or adjust the calibration data.

Usage of query.exe:

Query takes only one command line argument - the file to load.  It currently does no checking of the profile for validity, and prints the profile loading results out to the standard output.  Example of usage:

query myprofile.bpf

Loads the binary profile file myprofile.bpf to the driver.  For information on how to generate binary profile files, see profile.txt.

Query may also be used to clear the loaded profile or the calibration data.  Use:

query -c
to clear the currently loaded profile; and

query -a
to clear the calibration data.

Query may now be used to adjust the calibration values used by the system!  The following options are available.

  query -l <axis> <location> to disable value floating.
  query -f <axis> <location> to enable value floating.
  query -v <axis> <location> to load value from current stick position.
  query -z <axis> <location> <range> to set deadzone size.


Axis:  1 = X, 2 = Y, 3 = Throttle, 4 = Rudder, 5 = Rotary 1, 6 = Rotary 2
Location:  1 = min, 2 = center, 3 = max



Happy programming!

----------------------------------------

Known issues:

When installed this driver will create a 'HID-compliant joystick', 'HID-compliant keyboard', and 'HID-compliant mouse' entry in your Device Manager in addition to the X36 USB device.  This is normal.  DO NOT TRY TO REMOVE THESE EXTRA DEVICES.  I'm working on some infs to rename them to something useful but I haven't quite got them working yet.

----------------------------------------

Help me, it doesn't work!

Here are a few common questions and some possible solutions:

Q: I got to the 'Have Disk' screen, but it tells me your .inf doesn't have any drivers for my device!
A: Try letting Windows search for a newer driver, then give it the folder with the .inf.  Don't know why this works, but it does.

Q: It looks like I got your drivers installed, but none of the buttons work!
A: This is by design.  The buttons on my drivers are completely programmable, and the default program does nothing.  Try using query.exe to load 'default.bpf'.

Q: One or more of my axes are stuck or don't respond!
A: Use 'query -a' to clear the axis calibration data.  Then try moving all axes to their extremes.  If this still isn't working, try using 'calib.bat' or 'calib2.bat' to calibrate your stick.

Q: My throttle doesn't work in IL2!
A: For some reason it gets mapped to one of the rotaries.  Go through the IL2 control configuration setup and switch it back to the throttle (Z) axis.

----------------------------------------

Contact:

Send any and all questions, comments, feedback, and especially bug reports to:
	x36@vega.dyndns.org
	
Current updates will always be available at:
	http://vega.dyndns.org/x36/

More documentation and a support board is available from Ron Hunt's excellent site:
	http://saitekhelp.simhq.com
	

----------------------------------------


[Probably never] Coming features:
-> Real joystick calibration applet through control panel.
-> Opcodes to allow flow-control in scripts, eg. repeat mode support, break partway through command, etc.
-> A real installer. ;)


----------------------------------------
Changes since 2.105d:
Compiled using Windows XP/Server 2003 environment; may break on older OSes.
Added x64 build.


Changes since 2.105c:
Updated .INF to support new-model X45s. (Thanks to Eduard "Asterix" Huguet for this fix!)

Changes since 2.105b:
Patched to properly clear extended button structure.
Output reduced to 31 buttons to ensure compatibility in all games.  I should really parameterize this somewhere.

Changes since 2.105a:
Output extended to 63 DirectX buttons in attempt to correct issues with MW4.

Changes since 2.105:
Throttle, rudder, and both rotaries renamed when reported to system.  This should help detection in games.
Output extended to 32 DirectX buttons.

Changes since 2.104:
Fix to SMP protections on IRP capturing code - should correct random lockups on SMP machines.  Thanks to Tigera for filling in the missing link!


Changes since 2.103:
Several bluescreen fixes with axis chain support
Axis chains now sort properly on load
Axis chains now initialize properly to the correct position on load
Thanks to Ben Fears for getting me started on these!


Changes since 2.102:
Mouse thread and calibration writer thread explicitly re-enabled on PnP start IRP.  Thanks to Harv for finding this bug!
Throttle axis ID returned to default 'Slider' in attempt to correct detection in games.
Reporting mechanism changed again to correct Windows 9x interpretation errors.  Big thanks to Ken Fulton for all the help debugging this one!


Changes since 2.101:
Axis normalization extended - all axes now normalized to 16 bits.
Axis chains support rewritten.
Capability of switching axis chain added to configuration script.
Axis programmability interface rewritten.
'Axis profiles' merged into regular profiles v4.
Profile.txt updated to reflect new v4 profile format.
Pre-axis profile Query.exe restored.
Power management support for resuming from suspend added.
Calibration saving thread modified to support power management nicely.
Included profiles updated to v4.  Axis test profile included - binds buttons 1,2, and 3 to full, middle, and no throttle.


Changes since last 2.0delta revision:
Axis programmability support added.
Query.exe updated for axis programmability.
Added 'Axis Profiles.txt' with a description of axis programming model and bap file format.
Profile.txt changed to reflect v3 profile format (v1 and v2 are deprecated.)
Version numbering changed - new versions will be major.minor revision (ex 2.101)


Changes since 2.0delta5:
SMP locking fixed.  Should no longer lock up on SMP machines - this may also fix other inexplicable joystick locks.  Thanks to Scraptacular and Adorondi for locating this one and testing new builds!
Memory allocation rewritten to allow 65535 commands and configurations in prep for 2.1 release.
Profile v3 support added in prep for 2.1 release.
Updated readme with more useful calibration information.
Updated .inf to try and clear some Saitek Control Panel applet info.
Updated .inf to correct errors in the previous update and restore some strings.  Thanks to 16th_TailSpin for pointing this out!

Changes since 2.0delta4:
Fixed mouse movement support so it actually moves the mouse, now. :)  Thanks to Zdenek "RAT" Polach and Haphazard for reminding me to fix it.
Minor changes to .inf to (hopefully) eliminate some of the %HidClassDescription% stuff.

Changes since 2.0delta3:
Fixed state data clear on profile load.  Thanks to DewDog for pointing out this bug!

Changes since 2.0delta2:
Calibration data saved in registry when stick is unplugged or rebooted.
Save occurs every 10 seconds or so.
Documentation updated to discourage use of Windows calibration routines.

Changes since 2.0delta1:
Calibration support improved.  Now has centering and deadzone support.
Query.exe updated to support new features.
Changes to .inf file to make it clear that Dhauzimmer driver is installed.

Changes since 2.0gamma1:
Preliminary auto-calibration support added.
New switch added to Query to clear calibration data.


Changes since 2.0beta3a:
Mouse support added.  Four new opcodes in profile.txt.
Axis values passed by stick masked - should correct some EEPROM problems, but will always require calibration.


Changes since 2.0beta3:
X45 entries added to .inf.  Thanks to chist for providing the data!

Changes since 2.0beta2:
Profile load now safely clears existing profile - no possibility of crash
Profile load now clears keyboard and joystick buffers (can't believe I forgot this!)
Mode switch order now fully defined - executes release before press
-Thanks go to Haphazard for locating the above bugs!



Changes since 2.0beta1:
Fixed v2 profile loader - bug preventing profile loads
Fixed v2 profile loader - off-by-one in frobbable table
Frobbable table text file updated with off-by-one correction
Corrected frobbable table documentation (Launch and Fire B were reversed)
-Thanks go to Haphazard for locating the above bugs!


Changes since 2.0alpha1:
Added opcode to control Windows hat switch direction.
Added opcode to load new configuration for shifting.
Added profile version 2 to load shifting configurations.


Changes since 1.1b:
Entirely new memory structures.
Rewrote packet engine.
Removed registry support.
Added loader support.


Changes since 1.1a:

Fixed bug in new event dequeuing routine that causes lockups.  (Driver would dequeue the same event forever)


Changes since 1.1:

Code cleaned up quite a bit.
Internal engine modified to support new operations for 2.0 release. (Operations themselves not yet implemented.)
Shutdown issues resolved.
Shift bug fixed.


Changes since 1.0:

Numerous Windows 2000 issues resolved.   Driver now functions correctly with driver verifier.
LoadProfile support disabled due to W2K instability.



