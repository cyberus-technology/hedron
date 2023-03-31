# Hedron Hypervisor API Changelog

The main purpose of this document is to describe changes to the public API. It
might reflect other important internal changes, but that information may be incomplete.

*The changelog does not refer to Git tags or Git releases but to the API version
specified in `config.hpp / CFG_VER`.*

## API Version 10.0
- **Changed** `HC_CREATE_EC` now cannot be used to create vCPU-ECs anymore. The `vCPU` and the `Use APIC Access Page`
  bits of the `HC_CREATE_EC` system call must be zero now, otherwise Hedron will return with `BAD_PAR`.

## API Version 9.0
- **Changed** `HC_CREATE_VCPU` now takes an additional CPU parameter that indicates on which physical CPU the vCPU will
  run. Previously, the current CPU was assumed.

## API Version 8.4
- **New** We introduced the `HC_VCPU_CTRL_POKE` system call which a VMM can use to cause a VM exit that returns to user
  space as soon as possible.

## API Version 8.3
- **New** We introduced the `HC_CREATE_VCPU` and the `HC_VCPU_CTRL_RUN` system calls which a VMM can use
  to create the new vCPU kernel objects and also run them.

## API Version 8.2
- **New:** We introduced the `HC_IRQ_CTRL_ASSIGN_LVT` and `HC_IRQ_CTRL_MASK_LVT` system calls to allow
  manipulating LAPIC LVT entries. Currently only the thermal interrupt is accessible via this interface.

## API Version 8.1
- This version has no user-visible changes.

## API Version 8.0
- **Breaking:** The irq_ctrl system calls have incompatibly changed to allow for more suboperations.

## API Version 7.0
- **Breaking:** The HIP does no longer contain the per-CPU register dump. (33a90899)

## API Version 6.0
<!-- Full changelog: 79fc6e45..52590e1c -->
- **Breaking:** The `HC_ASSIGN_GSI` system call was removed. Its functionality is managed by the alternative
  `HC_IRQ_CTRL` system call. (f12c52e0)

## API Version 5.6
<!-- Full changelog: 771afd44..79fc6e45 -->
- **Changed:** The first parameter of the `HC_ASSIGN_GSI` system call (the bit after the system call number) must be
  set to `1` for backwards compatibility. `0` now results in a failed system call. (22034dea)
  *This might be breaking to others but to none of our known users so far.*

## API Version 5.5
<!-- Full changelog: adb99efc..771afd44 -->
- **New:** We introduced the new Kernel Page (KP) kernel object and the corresponding system calls `HC_CREATE_KP` and
  `HC_KP_CTRL`. (911ad097)

## API Version 5.4
<!-- Full changelog: 0c7a4496..adb99efc -->
- **Changed:** The object creation permission bits for a PD were shrunk to one single generic creation bit. (67ba72bb)
  *This might be breaking to others but to none of our known users so far.*

## API Version 5.3
*Versions 5.0 to 5.2 were skipped.*.
<!-- Full changelog: 95b5db56..0c7a4496 -->
- **Breaking:** The system call number of the first argument of a system call now uses bits `7..0` instead of `3..0`.
  (6307b9be)

## API Version 4.3
- **Fix:** The API feature flags inside the HIP are no longer truncated to `FEAT_VMX` and `FEAT_SVM`. (69e302ac)
