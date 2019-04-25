NOVA Microhypervisor
====================

This is the source code for the NOVA microhypervisor.

The NOVA microhypervisor combines microkernel and hypervisor functionality
and provides an extremely small trusted computing base for user applications
and virtual machines running on top of it. The microhypervisor implements a
capability-based authorization model and provides basic mechanisms for
virtualization, spatial and temporal separation, scheduling, communication,
and management of platform resources.

NOVA can be used with a multi-server environment that implements additional
operating-system services in user mode, such as device drivers, protocol
stacks, and policies. On machines with hardware virtualization features,
multiple unmodified guest operating systems can run concurrently on top of
the microhypervisor.

**This code is experimental and not feature complete. If it breaks, you get
  to keep both pieces.**


Supported platforms
-------------------

The NOVA microhypervisor runs on single- and multi-processor x86 machines
that support ACPI.

Recommended Intel CPUs are Intel Core processors starting with the Sandy Bridge
microarchitecture. AMD support is experimental and currently not actively used.

The virtualization features are available on:

- Intel CPUs with VMX and nested paging (EPT).

- AMD CPUs with SVM and nested paging (NPT),


Building from source code
-------------------------

You need the following tools to compile the source code:

- tup 0.7.8 or higher,
  available from http://gittup.org/tup/

- binutils 2.30 or higher,
  available from http://www.kernel.org/pub/linux/devel/binutils/

- gcc, available from http://gcc.gnu.org/
  - for x86_64: gcc 7.4.0 or higher


You can build a microhypervisor binary as follows:

    # Only needs to be done once
    tup init && tup variant configs/default.config
    
    tup


Booting
-------

The NOVA microhypervisor can be started from a multiboot-compliant
bootloader, such as GRUB or PXEGRUB. Here are some examples:

Boot from harddisk 0, partition 0

    title         NOVA
    kernel        (hd0,0)/boot/nova/hypervisor
    module        (hd0,0)/...
    ...

Boot from TFTP server aa.bb.cc.dd

    title         NOVA
    tftpserver    aa.bb.cc.dd
    kernel        (nd)/boot/nova/hypervisor
    module        (nd)/...
    ...


Command-Line Parameters
-----------------------

The following command-line parameters are supported for the microhypervisor.
They must be separated by spaces.

- *iommu*	- Enables DMA and interrupt remapping.
- *keyb*	- Enables the microhypervisor to drive the keyboard.
- *serial*	- Enables the microhypervisor to drive the serial console.
- *spinner*	- Enables event spinners.
- *nopcid*	- Disables TLB tags for address spaces.
- *novga*  	- Disables VGA console.
- *novpid* 	- Disables TLB tags for virtual machines.


License
-------

The NOVA source code is licensed under the GPL version 2.

```
Copyright (C) 2009-2011 Udo Steinberg <udo@hypervisor.org>
Economic rights: Technische Universitaet Dresden (Germany)

Copyright (C) 2012-2013 Udo Steinberg, Intel Corporation.

NOVA is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.

NOVA is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License version 2 for more details.
```


Contact
-------

Feedback and comments should be sent to hypervisor@cyberus-technology.de.
