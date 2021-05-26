# TSOEnabler

A kernel extension that enables total store ordering on Apple silicon, with semantics similar to x86_64's memory model. This is normally done by the kernel through modifications to a special register upon exit from the kernel for programs running under Rosetta 2; however, it is possible to enable this for arbitrary processes (on a per-thread basis, technically) as well by modifying the flag for this feature and letting the kernel enable it for us on. Setting this flag on certain processors can only be done on high-performance cores, so as a side effect of enabling TSO the kernel extension will also migrate your code off the efficiency cores permanently.

**This extension hardcodes offsets**, you may need to modify them.
Currently supported kernels are (select version in `TSOEnabler.c`);
- macOS Big Sur 11.2 Beta (20D5029f) kernel.release.t8020
- macOS Big Sur 11.3.1 (20E241) kernel.release.t8101

If your kernel is not supported, see instructions on how to find the `TSO_OFFSET` below.

## Installation

Supposedly, you should be able to use this if you build and sign the kernel extension (disabling SIP if you aren't using a KEXT signing certificate) and drag it into /Library/Extensions. A dialog should come up to prompt you to enable the extension in the Security & Privacy preferences pane, you allow it from there and restart, and it will be installed. In practice this can go wrong in many ways, and I have had luck by "resetting everything" and trying to install after doing the following:

1. Reboot into recovery.
2. Open the Startup Disk application. Your boot security should already be lowered to allow KEXTs from untrusted developers, toggle the checkbox anyways.
3. Quit Startup Disk and open Terminal.
4. Remove the kernel extension from the Data volume (likely /Volumes/Data/Library/Extensions/TSOEnabler.kext).
5. Run `kmutil trigger-panic-medic --volume-root /Volumes/<YourVolumeName>` (again, likely /Volumes/Data).

(The last two steps work around [bugs in macOS Big Sur beta](https://developer.apple.com/documentation/macos-release-notes/macos-big-sur-11-beta-release-notes)).

After that try installing the kernel extension again.

## Disabling SIP KEXT

An alternative to lowering the boot security is to disable SIP KEXT altogether.

1. Reboot into recovery.
2. Terminal.
3. Run `csrutil enable --without KEXT`

## Usage

Load the kernel extension with `sudo kextload /Library/Extensions/TSOEnabler.kext`. After that, the `kern.tso_enable` sysctl will be made available to you to read or write. For the reasons described above, you must enable this for all threads of your application if you would like it to work; doing so will also pin these to the high-performance cores of your processor.

## How to find the offsets

The `TSO_OFFSET` can be different for each kernel version and has to be found within the kernel.
For example, in macOS Big Sur 11.3.1 (20E241) kernel.release.t8101, the offset can be found like this:

1. Disassemble kernel code:
```
objdump -D /System/Library/Kernels/kernel.release.t8101 > kernel.lst

```
2. Find lines where `ACTLR_EL1` is accessed:
```
grep -C 20 ACTLR_EL1 kernel.lst | less -N
```

3. Look for code blocks where `TPIDR_EL1` is accessed immediately before like this:

```
 289785 fffffe000720f384: 81 d0 38 d5   mrs     x1, TPIDR_EL1
 289786 fffffe000720f388: 20 90 40 f9   ldr     x0, [x1, #288]   ;  <-------- This is the offset
 289787 fffffe000720f38c: 80 01 00 b4   cbz     x0, 0xfffffe000720f3bc
 289788 fffffe000720f390: 60 f8 3e d5   mrs     x0, S3_6_C15_C8_3
 289789 fffffe000720f394: 00 04 1f 12   and     w0, w0, #0x6
 289790 fffffe000720f398: e0 17 01 b9   str     w0, [sp, #276]
 289791 fffffe000720f39c: a0 f2 3e d5   mrs     x0, S3_6_C15_C2_5
 289792 fffffe000720f3a0: e0 4b 03 b9   str     w0, [sp, #840]
 289793 fffffe000720f3a4: 20 10 38 d5   mrs     x0, ACTLR_EL1
 289794 fffffe000720f3a8: 00 f8 7e 92   and     x0, x0, #0xfffffffffffffffd
 289795 fffffe000720f3ac: 00 f0 79 92   and     x0, x0, #0xffffffffffffff8f
 289796 fffffe000720f3b0: 9f 3f 03 d5   dsb     sy
 289797 fffffe000720f3b4: 20 10 18 d5   msr     ACTLR_EL1, x0
 289798 fffffe000720f3b8: df 3f 03 d5   isb
```

In this case the offset is **288**. Note the offset is given in decimal here (no 0x prefix).
