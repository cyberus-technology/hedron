# Hedron Hypervisor API Changelog

The main purpose of this document is to describe changes to the public API. It 
might reflect other important internal changes, but that information may be incomplete.

*The changelog does not refer to Git tags or Git releases but to the API version 
specified in `config.hpp / CFG_VER`.*

## API Version 7.0
<!-- Changelog temporarily removed because GitLab parses the range wrongly.  -->
Full changelog: 52590e1cbc110bfb7508cb824fb660f4ad5e83ba..HEAD
- **Breaking:** The HIP does no longer contain the per-CPU register dump. (33a908991f6d35c02f4179a8027ed55dc00e68c7)

## API Version 6.0
Full changelog: 79fc6e4575055957896894a8c56d150d4bd7668e..52590e1cbc110bfb7508cb824fb660f4ad5e83ba
- **Breaking:** The `HC_ASSIGN_GSI` system call was removed. Its functionality is managed by the alternative 
  `HC_IRQ_CTRL` system call. (f12c52e055dbeac67ced84ba5cb8f6681317a9dc)

## API Version 5.6
Full changelog: 771afd446db31595a3540944310602ba02fa08b8..79fc6e4575055957896894a8c56d150d4bd7668e
- **Changed:** The first parameter of the `HC_ASSIGN_GSI` system call (the bit after the system call number) must be 
  set to `1` for backwards compatibility. `0` now results in a failed system call. (22034dea945ebad1568e229edc2d7bb38ae8ec41)
  *This might be breaking to others but to none of our known users so far.*

## API Version 5.5
Full changelog: adb99efcd2cb92efa45d4440074d5d16312036d8..771afd446db31595a3540944310602ba02fa08b8
- **New:** We introduced the new Kernel Page (KP) kernel object and the corresponding system calls `HC_CREATE_KP` and 
  `HC_KP_CTRL`. (911ad09739a01af27eef268bbaa472553be57eb4)

## API Version 5.4
Full changelog: 0c7a44969ac31d3a216b30142a6f956ccd082b12..adb99efcd2cb92efa45d4440074d5d16312036d8
- **Changed:** The object creation permission bits for a PD were shrunk to one single generic creation bit. (67ba72bb81a042d5ac6f0eb1e89288731f4c912e)
  *This might be breaking to others but to none of our known users so far.*

## API Version 5.3
*Versions 5.0 to 5.2 were skipped.*.
Full changelog: 95b5db56490482e2794dd5c5de900aa919c7ced4..0c7a44969ac31d3a216b30142a6f956ccd082b12
- **Breaking:** The system call number of the first argument of a system call now uses bits `7..0` instead of `3..0`. 
  (6307b9be5a83ad4759e5d2557651dea6bbaf2f3f)

## API Version 4.3
- **Fix:** The API feature flags inside the HIP are no longer truncated to `FEAT_VMX` and `FEAT_SVM`. (69e302ac172980920a8aec0f00c2943bf9c9e711)
