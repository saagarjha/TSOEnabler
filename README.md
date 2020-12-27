# TSOEnabler

A kernel extension that enables total store ordering on Apple silicon, with semantics similar to x86_64's memory model. This is normally done by the kernel through modifications to a special register upon exit from the kernel for programs running under Rosetta 2; however, it is possible to enable this for arbitrary processes (on a per-thread basis, technically) as well by modifying the flag for this feature and letting the kernel enable it for us on. Setting this flag on certain processors can only be done on high-performance cores, so as a side effect of enabling TSO the kernel extension will also migrate your code off the efficiency cores permanently. **This extensions hardcodes offsets based on the macOS Big Sur 11.0 Beta (20A5374i) kernel.release.t8020. If you would like to run it on any other computer, you will need to modify these.**

## Installation

Supposedly, you should be able to use this if you build and sign the kernel extension (disabling SIP if you aren't using a KEXT signing certificate) and drag it into /Library/Extensions. A dialog should come up to prompt you to enable the extension in the Security & Privacy preferences pane, you allow it from there and restart, and it will be installed. In practice this can go wrong in many ways, and I have had luck by "resetting everything" and trying to install after doing the following:

1. Reboot into recovery.
2. Open the Startup Disk application. Your boot security should already be lowered to allow KEXTs from untrusted developers, toggle the checkbox anyways.
3. Quit Startup Disk and open Terminal.
4. Remove the kernel extension from the Data volume (likely /Volumes/Data/Library/Extensions/TSOEnabler.kext).
5. Run `kmutil trigger-panic-medic --volume-root /Volumes/<YourVolumeName>` (again, likely /Volumes/Data).

(The last two steps work around [bugs in macOS Big Sur beta](https://developer.apple.com/documentation/macos-release-notes/macos-big-sur-11-beta-release-notes)).

After that try installing the kernel extension again.

## Usage

Load the kernel extension with `sudo kextload /Library/Extensions/TSOEnabler.kext`. After that, the `kern.tso_enable` sysctl will be made available to you to read or write. For the reasons described above, you must enable this for all threads of your application if you would like it to work; doing so will also pin these to the high-performance cores of your processor.