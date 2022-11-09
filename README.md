# Hedron Hypervisor

The Hedron hypervisor combines microkernel and hypervisor functionality
and provides an extremely small trusted computing base for user applications
and virtual machines running on top of it. The hypervisor implements a
capability-based authorization model and provides basic mechanisms for
virtualization, spatial and temporal separation, scheduling, communication,
and management of platform resources.

Hedron has to be used with a multi-server environment that implements
operating-system services in user mode, such as device drivers,
protocol stacks, and policies. On machines with hardware
virtualization features, multiple unmodified guest operating systems
can run concurrently on top of the hypervisor facilitated by a
virtual machine monitor running in user space.

Hedron is currently used as the core of the [Secure Virtual
Platform](https://www.cyberus-technology.de/products/svp.html) by
[Cyberus Technology GmbH](https://www.cyberus-technology.de/).

Hedron is open source under the [GPLv2](./LICENSE) license. Please
consider talking to us before using it in any production system as
there are important caveats that may not be very well documented.

## Changelog

A changelog is provided in [CHANGELOG.md](CHANGELOG.md).

## Building

### Nix (recommended)

If you are only interested in building Hedron without any hassle, you
can do so using [Nix](https://nixos.org/) on most Linux
distributions. This recreates exactly the same binaries we test.

After [installing Nix](https://nixos.org/download.html), build Hedron
using:

```bash
$ nix-build nix/release.nix -A hedron.builds.default-release # For a release build
$ nix-build nix/release.nix -A hedron.builds.default-debug   # For a debug build
```

There is a shorthand for building a release build:

```bash
$ nix-build
```

The hypervisor is then found in `result/`. With Nix available, other
build options for developers become available. See the documentation
in `nix/release.nix` for details.

### Manual Build (for developers)

You need the following tools to compile the hypervisor:

- cmake 3.13 or higher,
- binutils 2.30 or higher,
- gcc 10.0.0 or higher,
- or alternatively, clang 12.0 or higher.

To build and run the unit tests (optional), you need:

- pkg-config,
- Catch2.

You can build a hypervisor binary as follows:

```sh
# Only needs to be done once
% mkdir -p build
% cd build ; cmake ..

# Build the hypervisor and execute unit tests
build % make
build % make test
```

Building unit tests can be avoided by passing `-DBUILD_TESTING=OFF` to
`cmake`. Additional configuration flags can be configured using
`ccmake` or other CMake frontends:

```sh
build % ccmake .
```

## Running

### Supported platforms

The Hedron hypervisor runs on single- and multi-processor x86
machines that support ACPI, XSAVE and FSGSBASE.

Recommended Intel CPUs are Intel Core processors starting with the Ivy
Bridge microarchitecture. The virtualization features are available on
Intel CPUs with VMX and nested paging (EPT).

Intel Atom CPUs (also labeled Pentium Silver or Celeron) should work
starting with the Goldmont Plus microarchitecture, but are not
actively tested. Consider running Hedron on Atom systems experimental.

AMD support still exists in the code, but is currently not in a
production-ready state and is not actively tested. Do not use Hedron
on AMD systems unless you are a developer and expect to fix things.

### Boot

The Hedron hypervisor can be started from a multiboot-compliant
bootloader, such as GRUB or iPXE. Hedron supports Multiboot 1 and 2
(for UEFI). Here are some examples that assume a Hedron-compatible
`roottask` binary.

Boot as a Multiboot2 payload in Grub2:

```
multiboot2 hypervisor-x86_64 serial novga
module2    roottask
```

Boot as a Multiboot1 payload with iPXE via TFTP:

```
kernel tftp://${next-server}/hypervisor.elf32 serial novga
initrd tftp://${next-server}/roottask
```

### Command-Line Parameters

Hedron supports the following command-line parameters. They must be
separated by spaces.

- *iommu*	- Enables DMA and interrupt remapping.
- *serial*	- Enables the hypervisor to drive the serial console.
- *nopcid*	- Disables TLB tags for address spaces.
- *novga*  	- Disables VGA console.
- *novpid* 	- Disables TLB tags for virtual machines.

## Developing

### Hedron (Cyberus-internal)

Please check the internal developer wiki for up-to-date instructions.

### Hedron (External)

The [Hedron Github
 repository](https://github.com/cyberus-technology/hedron/) is a
 mirror the Cyberus Technology internal Hedron repository. Please
 contact us (see below) if you want to contribute to Hedron. We are
 not actively monitoring PRs and issues on Github.

### User Space Applications

Hedron's system calls are documented in the [Kernel Interface
documentation](./docs/kernel-interface.md). This document is
unfortunately not complete yet.

## Credits

Hedron is derived from the NOVA hypervisor developed by Udo
Steinberg. While NOVA and Hedron are still close in spirit, the last
common commit dates from 2015. Since then Hedron has been steadily
modernized with a focus on simplicity, testability, and support for
modern virtualization features. Over the years, Hedron also adopted
patches by Genode Labs developed as part of their NOVA fork.

## Contact

Please send feedback and comments to hypervisor@cyberus-technology.de.
