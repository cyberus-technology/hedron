# Preprocess the linker script
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/hypervisor-x86_64.ld
  MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/hypervisor.ld
  COMMAND ${CMAKE_CXX_COMPILER}
  ARGS
  -x c -E ${CMAKE_CURRENT_SOURCE_DIR}/hypervisor.ld -P
  -o ${CMAKE_CURRENT_BINARY_DIR}/hypervisor-x86_64.ld
  -I ${CMAKE_CURRENT_SOURCE_DIR}/../include
  VERBATIM
  )

add_executable(hypervisor hypervisor-x86_64.ld
  # Assembly sources
  entry.S  start.S

  # C++ sources
  acpi.cpp acpi_dmar.cpp acpi_fadt.cpp acpi_hpet.cpp acpi_madt.cpp
  acpi_mcfg.cpp acpi_rsdp.cpp acpi_rsdt.cpp acpi_table.cpp avl.cpp
  bootstrap.cpp buddy.cpp cmdline.cpp console.cpp console_serial.cpp
  console_vga.cpp counter.cpp cpu.cpp cpulocal.cpp dmar.cpp dpt.cpp ec.cpp
  ec_exc.cpp ec_svm.cpp ec_vmx.cpp ept.cpp fpu.cpp gdt.cpp gsi.cpp hip.cpp
  hpet.cpp hpt.cpp idt.cpp init.cpp ioapic.cpp lapic.cpp
  mca.cpp mdb.cpp memory.cpp mtrr.cpp pci.cpp pd.cpp pt.cpp
  rcu.cpp regs.cpp sc.cpp si.cpp slab.cpp sm.cpp space.cpp
  space_mem.cpp space_obj.cpp space_pio.cpp string.cpp svm.cpp
  syscall.cpp timeout_budget.cpp timeout.cpp timeout_hypercall.cpp
  tss.cpp utcb.cpp vmx.cpp
  )

add_custom_command(
  TARGET hypervisor
  POST_BUILD
  COMMAND ${CMAKE_OBJCOPY}
  ARGS -SO elf32-i386 hypervisor hypervisor.elf32
  )

if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set_property(TARGET hypervisor PROPERTY INTERPROCEDURAL_OPTIMIZATION True)
endif()

target_compile_options(hypervisor PRIVATE
  # Optimization
  $<IF:$<CONFIG:Debug>,-Og,-Os>
  -m64 -march=core2 -mcmodel=kernel -mno-red-zone -fno-PIC

  # Language
  -nostdinc++ -ffreestanding
  -fdata-sections -ffunction-sections -fomit-frame-pointer
  -fno-asynchronous-unwind-tables -fno-exceptions -fno-rtti -fno-threadsafe-statics
  -fno-stack-protector
  -fvisibility-inlines-hidden
  -fdiagnostics-color=auto
  -mno-sse
  -mno-mmx
  -mno-3dnow

  # Warnings
  -Wall -Wextra -Wcast-align -Wcast-qual -Wconversion
  -Wdisabled-optimization -Wformat=2 -Wmissing-format-attribute
  -Wmissing-noreturn -Wpacked -Wpointer-arith -Wredundant-decls
  -Wshadow -Wwrite-strings -Wctor-dtor-privacy -Wno-non-virtual-dtor
  -Wold-style-cast -Woverloaded-virtual -Wsign-promo
  -Wstrict-overflow -Wvolatile-register-var
  -Wzero-as-null-pointer-constant
  )

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  target_compile_options(hypervisor PRIVATE
    $<$<CONFIG:Debug>:-Werror>
    -Wlogical-op
    -Wstrict-null-sentinel
    -Wframe-larger-than=256
    )
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  target_compile_options(hypervisor PRIVATE
    -no-integrated-as
    -Wno-sign-conversion
    -Wno-unused-private-field
    -Wno-missing-noreturn	# See NORETURN_GCC in include/compiler.h
    -Wno-implicit-int-conversion
    -Wno-static-self-init
    )
endif()

find_package(Git QUIET)
if(GIT_FOUND AND EXISTS "${PROJECT_SOURCE_DIR}/.git")
  # This command only runs during configuration time and will embed
  # the git revision from that time until re-configuration.
  execute_process(COMMAND
    "${GIT_EXECUTABLE}" rev-parse --short=7 HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE GIT_VER
    OUTPUT_STRIP_TRAILING_WHITESPACE
    )
else()
  set(GIT_VER "00000000")
endif()

target_link_options(hypervisor PRIVATE
  -static
  -nostdlib
  -Wl,--defsym=GIT_VER=0x${GIT_VER},--gc-sections,--warn-common,-n,--build-id=none
  -Wl,--defsym=CONFIG_KERNEL_MEMORY=64M
  -Wl,-T ${CMAKE_CURRENT_BINARY_DIR}/hypervisor-x86_64.ld
  )
