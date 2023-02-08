# System Call Binary Interface for x86_64

## Register Usage

System call parameters are passed in registers. The following register names are used in the System Call Reference.

### Input Parameters

| *Logical Name* | *Actual Register* |
|----------------|-------------------|
| `ARG1`         | `RDI`             |
| `ARG2`         | `RSI`             |
| `ARG3`         | `RDX`             |
| `ARG4`         | `RAX`             |
| `ARG5`         | `R8`              |

### Output Parameters

| *Logical Name* | *Actual Register* |
|----------------|-------------------|
| `OUT1`         | `RDI`             |
| `OUT2`         | `RSI`             |

## Modified Registers

Only registers listed above are modified by the kernel. Note that `RCX` and `R11` are modified by
the CPU as part of executing the `SYSCALL` instruction.

## Hypercall Numbers

Hypercalls are identified by these values.

| *Constant*                         | *Value* |
|------------------------------------|---------|
| `HC_CALL`                          | 0       |
| `HC_REPLY`                         | 1       |
| `HC_CREATE_PD`                     | 2       |
| `HC_CREATE_EC`                     | 3       |
| `HC_CREATE_SM`                     | 6       |
| `HC_REVOKE`                        | 7       |
| `HC_PD_CTRL`                       | 8       |
| `HC_EC_CTRL`                       | 9       |
| `HC_SM_CTRL`                       | 12      |
| `HC_ASSIGN_PCI`                    | 13      |
| `HC_MACHINE_CTRL`                  | 15      |
| `HC_CREATE_KP`                     | 16      |
| `HC_KP_CTRL`                       | 17      |
| `HC_IRQ_CTRL`                      | 18      |

## Hypercall Status

Most hypercalls return a status value in OUT1. The following status values are defined:

| *Status*  | *Value* | *Description*                                                            |
|-----------|---------|--------------------------------------------------------------------------|
| `SUCCESS` | 0       | The operation completed successfully                                     |
| `TIMEOUT` | 1       | The operation timed out                                                  |
| `ABORT`   | 2       | The operation was aborted                                                |
| `BAD_HYP` | 3       | An invalid hypercall was called                                          |
| `BAD_CAP` | 4       | A hypercall referred to an empty or otherwise invalid capability         |
| `BAD_PAR` | 5       | A hypercall used invalid parameters                                      |
| `BAD_FTR` | 6       | An invalid feature was requested                                         |
| `BAD_CPU` | 7       | A portal capability was used on the wrong CPU                            |
| `BAD_DEV` | 8       | An invalid device ID was passed                                          |
| `OOM`     | 9       | The hypervisor ran out of memory                                         |
| `BUSY`    | 10      | The operation couldn't complete successfully because a resource is busy. |
