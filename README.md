# TSOEnabler

A kernel extension that enables total store ordering on Apple silicon, with semantics similar to x86_64's memory model. This is normally done by the kernel through modifications to a special register upon exit from the kernel for programs running under Rosetta 2; however, it is possible to enable this for arbitrary processes (on a per-thread basis, technically) as well by modifying the flag for this feature and letting the kernel enable it for us on. **This extension is designed to work on the M1 (t8101) kernel, where it attempts to automatically detect certain offsets from the kernel image.** If you are looking for the old code for the A12Z (t8020) kernel, it's available [on the t8020 branch](https://github.com/saagarjha/TSOEnabler/tree/t8020).

## Installation

Supposedly, you should be able to use this if you build and sign the kernel extension (disabling SIP if you aren't using a KEXT signing certificate) and drag it into /Library/Extensions. A dialog should come up to prompt you to enable the extension in the Security & Privacy preferences pane, you allow it from there and restart, and it will be installed. (If you're not seeing it, the permissions on the extension might be wrong: try a `chown -R root:wheel`.) In practice this can go wrong in many ways, and I have had luck by "resetting everything" and trying to install after doing the following:

1. Reboot into recovery.
2. Open the Startup Disk application. Your boot security should already be lowered to allow KEXTs from untrusted developers, toggle the checkbox anyways.
3. Quit Startup Disk and open Terminal.
4. Remove the kernel extension from the Data volume (likely /Volumes/Data/Library/Extensions/TSOEnabler.kext).
5. Run `kmutil trigger-panic-medic --volume-root /Volumes/<YourVolumeName>` (again, likely /Volumes/Data).

(The last two steps work around [bugs in macOS Big Sur beta](https://developer.apple.com/documentation/macos-release-notes/macos-big-sur-11-beta-release-notes)).

After that try installing the kernel extension again.

## Usage

Load the kernel extension with `sudo kextload /Library/Extensions/TSOEnabler.kext`. After that, the `kern.tso_enable` sysctl will be made available to you to read or write. For the reasons described above, you must enable this for all threads of your application if you would like it to work; doing so will also pin these to the high-performance cores of your processor. [A shared library](https://github.com/saagarjha/TSOEnabler/blob/master/enabletso/enabletso.c) that interposes pthreads to automatically enable TSO is also provided for convenience.
