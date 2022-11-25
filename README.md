# nohv

`nohv` is a kernel driver for detecting Intel VT-x hypervisors. It is useful for benchmarking your hypervisor against common vm-detections.

## Usage

To clone the repo:

```powershell
git clone --recursive https://github.com/jonomango/nohv.git
```

`nohv` is a Windows driver built with MSVC. It requires 
[Visual Studio](https://visualstudio.microsoft.com/downloads/) and the
[WDK](https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk) for compilation.

Once compiled, `nohv.sys` must be loaded with SEH support. This means that you can't manual map 
the driver (unless you're a wizard) since it'll crash the moment an exception is thrown. I recommend 
restarting Windows while holding SHIFT and disabling Driver Signature Enforcement, then load the
driver normally ([OSR Loader](https://www.osronline.com/article.cfm%5Earticle=157.htm) if you're lazy) 
and hope you don't BSOD :smiley:.

## Remarks

This is a fairly old project of mine and it's missing a lot of common detections (such as 
[NMI checks](https://www.unknowncheats.me/forum/c-and-c-/390593-vm-escape-via-nmi.html)). Also this
**WILL** bluescreen you if your hypervisor sucks. Make sure to test this **BEFORE** loading your
hypervisor, as well as after.
