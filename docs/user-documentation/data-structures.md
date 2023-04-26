# Data Structures

## Hypervisor Information Page (HIP)

The HIP is a region of memory exported by the hypervisor to the
roottask. It contains system information that the roottask can
otherwise not obtain.

Check `include/hip.hpp` for its layout.

| *Field Name*       | *Description*                                                                      |
|--------------------|------------------------------------------------------------------------------------|
| `signature`        | Magic value to recognize the HIP: 'HDRN' in little-endian (`0x4e524448`)           |
| `checksum`         | The HIP is valid if 16bit-wise addition of the HIP contents produces a value of 0. |
| `length`           | Length of the HIP in bytes. This includes all CPU and memory descriptors.          |
| `cpu_offset`       | Offset of the first CPU descriptor in bytes, relative to the HIP base.             |
| `cpu_size`         | Size of a CPU descriptor in bytes.                                                 |
| `ioapic_offset`    | Offset of the first IOAPIC descriptor in bytes, relative to the HIP base.          |
| `ioapic_size`      | Size of an IOAPIC descriptor in bytes.                                             |
| `mem_offset`       | Offset of the first memory descriptor in bytes, relative to the HIP base.          |
| `mem_size`         | Size of a memory descriptor in bytes.                                              |
| `api_flg`          | A bitmask of feature flags. See Features section below.                            |
| `api_ver`          | The API version. See API Version section below.                                    |
| `sel_num`          | Number of available capability selectors in each object space.                     |
| `sel_exc`          | Number of selectors used for exception handling.                                   |
| `sel_vmi`          | Number of selectors for VM exit handling.                                          |
| `num_user_vectors` | The number of interrupt vectors usable in `irq_ctrl` system calls.                 |
| `cfg_page`         | The system page size.                                                              |
| `cfg_utcb`         | The size of an UTCB.                                                               |
| `freq_tsc`         | The frequency of the x86 Timestamp Counter (TSC) in kHz.                           |
| `freq_bus`         | The bus frequency (LAPIC timer frequency) in kHz.                                  |
| `pci_bus_start`    | First bus number that is served by the MMCONFIG region pointed to by `mmcfg_base`. |
| `mcfg_base`        | The physical address of the first MMCONFIG region for PCI segment 0.               |
| `mcfg_size`        | The size of the MMCONFIG region pointed to by `mcfg_base`.                         |
| `dmar_table`       | The physical address of the ACPI DMAR table.                                       |
| `hpet_base`        | The physical address of the HPET MMIO registers.                                   |
| `cap_vmx_sec_exec` | The secondary VMX execution control capabilities (or 0 if not supported).          |
| `xsdt_rdst_table`  | The physical address of either the XSDT, or if not available, the RDST ACPI table. |
| `pm1a_cnt`         | The location of the ACPI PM1a_CNT register block as ACPI Generic Address.          |
| `pm1b_cnt`         | The location of the ACPI PM1b CNT register block as ACPI Generic Address.          |
| `bsp_lapic_svr`    | The value of the LAPIC SVR register on the BSP when Hedron was booted.             |
| `bsp_lapic_lint0`  | The value of the LAPIC LINT0 LVT entry on the BSP when Hedron was booted.          |

Additional fields are not yet documented. Please consider documenting
them.

### API Version

The Hedron API uses semantic versioning. The low 12 bits of the
`api_ver` field is the minor version. Increases of the minor version
happen for backward compatible changes. The upper bits are the major
version. It is increased for backward incompatible changes.

### Features

This section describes the features of the `api_flg` field in the HIP.

| *Name* | *Bit* | *Description*                                                                               |
|--------|-------|---------------------------------------------------------------------------------------------|
| IOMMU  | 0     | The platform provides an IOMMU, and the feature has been activated.                         |
| VMX    | 1     | The platform supports Intel Virtual Machine Extensions, and the feature has been activated. |
| SVM    | 2     | The platform supports AMD Secure Virtual Machine, and the feature has been activated.       |
| UEFI   | 3     | Hedron was booted via UEFI.                                                                 |

**Note**: Support for AMD SVM and the IOMMU have been removed. Either of these features will never be reported
by Hedron, even on a system supporting it.

## Capabilities

A capability is a reference to a kernel object plus associated access
permissions. Capabilities are opaque and immutable to the user. They
cannot be inspected, modified or addressed directly. Instead user
programs access a capability via a capability selector.

There are three different kinds of capabilities:

- memory capabilites,
- port I/O capabilites, and
- object capabilities.

Each kind of capability has unique capability selectors that form the
respective capability _space_ for this kind. Which kind of
capabilities a selector refers to depends on context.

Memory and port I/O capabilities are created by the
hypervisor. Object capabilities can be created using `create_*`
system calls, see below. When capabilities are created they hold all
permissions. When capabilities are delegated their permissions can be
downgraded, but never upgraded.

## Capability Permissions

Each type of object capability has a different set of permission
bits. There are five permission bits in total. Permissions marked with
0 are unused and should be specified as zero.

### Memory Capabilities

| 4 | 3 | 2 | 1 | 0 |
|---|---|---|---|---|
| 0 | 0 | x | w | r |

Memory capabilities can control eXecute, Write, and Read
permissions. Depending on the platform execute permissions may imply
read permissions.

### Port I/O Capabilities

| 4 | 3 | 2 | 1 | 0 |
|---|---|---|---|---|
| 0 | 0 | 0 | 0 | a |

I/O port capabilities only have a single permission that allows
reading and writing the I/O port.

### Protection Domain (PD) Object Capability

| 4 | 3 | 2 | 1 | 0      |
|---|---|---|---|--------|
| 0 | 0 | 0 | 0 | create |

A Protection Domain capability has one permission bit that decides whether
the PD capability can be used to create object capabilities. If the `create`
bit is set, this PD capability can be used to create object capabilities with a
`create_*` system call.

### Execution Context (EC) Object Capability

| 4 | 3  | 2  | 1 | 0  |
|---|----|----|---|----|
| 0 | pt | sc | 0 | ct |

If `ct` is set, the `ec_ctrl` system call is permitted.

If `sc` is set, `create_sc` is allowed to bind a scheduling context.

if `pt` is set, `create_pt` can bind a portal.

### Scheduling Context (SC) Object Capability

| 4 | 3 | 2 | 1 | 0  |
|---|---|---|---|----|
| 0 | 0 | 0 | 0 | ct |

If `ct` is set, the `sc_ctrl` system call is permitted.

### Portal (PT) Object Capability

| 4 | 3 | 2 | 1    | 0  |
|---|---|---|------|----|
| 0 | 0 | 0 | call | ct |

If `ct` is set, the `pt_ctrl` system call is permitted.

If `call` is set, the portal can be traversed using `call`.

### Semaphore (SM) Object Capability

| 4 | 3 | 2 | 1    | 0  |
|---|---|---|------|----|
| 0 | 0 | 0 | down | up |

If `up` is set, the `sm_ctrl` system call is permitted to do an "up" operation.

If `down` is set, the `sm_ctrl` system call is permitted to do a "down" operation.

### Kernel Page (KP) Object Capability

| 4 | 3 | 2 | 1 | 0  |
|---|---|---|---|----|
| 0 | 0 | 0 | 0 | ct |

If `ct` is set, the `kp_ctrl` system call is permitted.

## Capability Range Descriptor (CRD)

A CRD is a 64-bit value that describes a range of capabilities of a
specific kind. The three kinds of capabilities have slightly different
layouts for their CRDs.

### Null CRD

A null CRD does not refer to any capabilites.

| *Field*     | *Content* | *Description*                   |
|-------------|-----------|---------------------------------|
| `CRD[1:0]`  | Kind      | Needs to be `0` for a Null CRD. |
| `CRD[63:2]` | Ignored   | Should be set to zero.          |

### Memory CRD

A memory CRD refers to pages of address space.

| *Field*      | *Content*   | *Description*                                                                                    |
|--------------|-------------|--------------------------------------------------------------------------------------------------|
| `CRD[1:0]`   | Kind        | Needs to be `1` for memory capabilities.                                                         |
| `CRD[6:2]`   | Permissions | See memory capability permissions above.                                                         |
| `CRD[11:7]`  | Order       | Describes the size of this memory region as power-of-two of pages of memory.                     |
| `CRD[63:12]` | Base        | The page number of the first page in the region. This number must be naturally aligned by order. |

### Port I/O CRD

A port I/O CRD refers to a range of x86 I/O ports.

| *Field*      | *Content*   | *Description*                                                             |
|--------------|-------------|---------------------------------------------------------------------------|
| `CRD[1:0]`   | Kind        | Needs to be `2` for port I/O capabilities.                                |
| `CRD[6:2]`   | Permissions | See I/O port capability permissions above.                                |
| `CRD[11:7]`  | Order       | Describes the size of the range as power-of-two of individual I/O ports.  |
| `CRD[63:12]` | Base        | The address of the first I/O port. It must be naturally aligned by order. |

### Object CRD

| *Field*      | *Content*   | *Description*                                                                            |
|--------------|-------------|------------------------------------------------------------------------------------------|
| `CRD[1:0]`   | Kind        | Needs to be `3` for object capabilities.                                                 |
| `CRD[6:2]`   | Permissions | Permissions are specific to each object capability type. See the relevant section above. |
| `CRD[11:7]`  | Order       | Describes the size of this region as power-of-two of individual capabilities.            |
| `CRD[63:12]` | Base        | The first capability selector. This number must be naturally aligned by order.           |

## Delegate Flags

Delegate flags are specified as an unsigned 64-bit value. The flags
describe how capabilities are be transferred. It is used in the
`pd_ctrl_delegate` syscall.

| *Field*           | *Content*  | *Description*                                                                                                  |
|-------------------|------------|----------------------------------------------------------------------------------------------------------------|
| `DLGFLAGS[0]`     | Type       | Must be `1`                                                                                                    |
| `DLGFLAGS[7:1]`   | Reserved   | Must be `0`                                                                                                    |
| `DLGFLAGS[8]`     | !Host      | Mapping needs to go into (0) / not into (1) host page table. Only valid for memory and I/O delegations.        |
| `DLGFLAGS[9]`     | Guest      | Mapping needs to go into (1) / not into (0) guest page table / IO space. Valid for memory and I/O delegations. |
| `DLGFLAGS[10]`    | Ignored    | Should be zero for future compatibility.                                                                       |
| `DLGFLAGS[11]`    | Hypervisor | Source is actually hypervisor PD. Only valid when used by the roottask, silently ignored otherwise.            |
| `DLGFLAGS[63:12]` | Hotspot    | The hotspot used to disambiguate send and receive windows.                                                     |

## User Thread Control Block (UTCB)

UTCBs belong to Execution Contexts. Each EC has an associated UTCB. It is
used to send and receive message data and architectural state via IPC.

The UTCB is 4KiB in size. It's detailed layout is given in
`include/utcb.hpp`.

### TSC Timeout

The `tsc_timeout` utcb field in addition to the `Mtd::TSC_TIMEOUT` MTD bit
allow to specify a relative timeout value. The `tsc_timeout` value counts down
at a rate proportional to the TSC. After it reaches zero it stops counting and
a VM exit is generated.

Due to architectural restrictions the `tsc_timeout` value will be saved in a
32bit value. This means that if a user specifies a timeout larger than what
fits into 32bits the timer might fire earlier than expected. To get the timeout
delivered at the intended time the user has to re-arm the timer with the actual
time left.

On the other hand the internal precision is lower than what can be specified
and the timeout is rounded up to the internal precision.

Further, if the user does not program the TSC timeout there might be a TSC
timeout related spurious VM exit which can be ignored.

## Virtual LAPIC (vLAPIC) Page

vLAPIC pages belong to vCPUs. A vCPU has exactly one
vLAPIC page.

A vLAPIC page is exactly 4KiB in size. The content of the vLAPIC page
is given by the Intel Architecture. The Intel Software Development
Manual Vol. 3 describes its content and layout in the "APIC
Virtualization and Virtual Interrupts" chapter. In the Intel
documentation, this page is called "virtual-APIC page".

## APIC Access Page

The APIC Access Page is a page of memory that is used to mark the
location of the Virtual LAPIC in a VM. All vCPUs use the APIC Access Page.

The Intel Software Development Manual Vol. 3 gives further information
on how the APIC Access Page changes the operation of Intel VT in the
"APIC Virtualization and Virtual Interrupts" chapter. In the Intel
documentation, this page is called "APIC-access page".
