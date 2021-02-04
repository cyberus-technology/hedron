set(HEAP_SIZE_MB 256 CACHE STRING "The amount of hypervisor heap space in MiB.")

# Retpolines have a small impact on usual workloads, so we enable them
# by default. Disabling retpolines opens up the possibility to do
# Spectre v2 attacks against the hypervisor.
option(ENABLE_RETPOLINE "Enable retpolines for Spectre v2 mitigation." ON)

add_executable(hypervisor
  # Assembly sources
  entry.S  start.S

  # C++ sources
  acpi.cpp acpi_dmar.cpp acpi_fadt.cpp acpi_hpet.cpp acpi_madt.cpp
  acpi_mcfg.cpp acpi_rsdp.cpp acpi_rsdt.cpp acpi_table.cpp avl.cpp
  bootstrap.cpp buddy.cpp cmdline.cpp console.cpp console_serial.cpp
  console_vga.cpp cpu.cpp cpulocal.cpp dmar.cpp dpt.cpp ec.cpp
  ec_exc.cpp ec_svm.cpp ec_vmx.cpp ept.cpp fpu.cpp gdt.cpp gsi.cpp hip.cpp
  hpet.cpp hpt.cpp idt.cpp init.cpp ioapic.cpp lapic.cpp
  mca.cpp mdb.cpp memory.cpp msr.cpp mtrr.cpp pci.cpp pd.cpp pt.cpp
  rcu.cpp regs.cpp sc.cpp si.cpp slab.cpp sm.cpp space.cpp
  space_mem.cpp space_obj.cpp space_pio.cpp string.cpp suspend.cpp svm.cpp
  syscall.cpp timeout_budget.cpp timeout.cpp timeout_hypercall.cpp
  tss.cpp utcb.cpp vlapic.cpp vmx.cpp
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
    $<$<BOOL:${ENABLE_RETPOLINE}>:-mindirect-branch=thunk>
    )
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  target_compile_options(hypervisor PRIVATE
    -Wno-sign-conversion
    -Wno-unused-private-field
    -Wno-missing-noreturn	# See NORETURN_GCC in include/compiler.h
    -Wno-implicit-int-conversion
    -Wno-static-self-init
    $<$<BOOL:${ENABLE_RETPOLINE}>:-mretpoline>
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

# Preprocess and use the custom linker script
set(HYPERVISOR-LINKER-SOURCE ${CMAKE_CURRENT_SOURCE_DIR}/hypervisor.ld)
set(HYPERVISOR-LINKER-SCRIPT ${CMAKE_CURRENT_BINARY_DIR}/hypervisor.ld)

# In order to let the build system reason about the dependencies
# (header files) of preprocessed linker scripts, cmake needs to
# determine those dependencies so we don't have to specify them
# manually.
#
# Unfortunately, there is a catch:
# - IMPLICIT_DEPENDS works for Makefiles and is ignored by all others
# - DEPFILE works for Ninja, but causes an error with Makefiles (sigh)
#
# The only reasonable solution right now is to duplicate this custom
# command depending on the generator.
if(CMAKE_GENERATOR STREQUAL "Ninja")
  add_custom_command(
    OUTPUT ${HYPERVISOR-LINKER-SCRIPT}
    MAIN_DEPENDENCY ${HYPERVISOR-LINKER-SOURCE}
    DEPFILE ${HYPERVISOR-LINKER-SCRIPT}.d
    COMMAND ${CMAKE_CXX_COMPILER}
    ARGS
    -x c -E ${HYPERVISOR-LINKER-SOURCE} -P
    -o ${HYPERVISOR-LINKER-SCRIPT}
    -I ${CMAKE_SOURCE_DIR}/include
    )
endif()
if(CMAKE_GENERATOR STREQUAL "Unix Makefiles")
  add_custom_command(
    OUTPUT ${HYPERVISOR-LINKER-SCRIPT}
    MAIN_DEPENDENCY ${HYPERVISOR-LINKER-SOURCE}
    IMPLICIT_DEPENDS CXX ${HYPERVISOR-LINKER-SOURCE}
    COMMAND ${CMAKE_CXX_COMPILER}
    ARGS
    -x c -E ${HYPERVISOR-LINKER-SOURCE} -P
    -o ${HYPERVISOR-LINKER-SCRIPT}
    -I ${CMAKE_SOURCE_DIR}/include
    )
endif()

add_custom_target(hypervisor-ld-script-generated DEPENDS ${HYPERVISOR-LINKER-SCRIPT})

add_dependencies(hypervisor hypervisor-ld-script-generated)
set_target_properties(hypervisor PROPERTIES LINK_DEPENDS ${HYPERVISOR-LINKER-SCRIPT})
target_link_options(hypervisor PRIVATE
  -static
  -nostdlib
  -Wl,--defsym=GIT_VER=0x${GIT_VER},--gc-sections,--warn-common,-n,--build-id=none
  -Wl,--defsym=CONFIG_KERNEL_MEMORY=${HEAP_SIZE_MB}M
  -Wl,-T ${HYPERVISOR-LINKER-SCRIPT}
  )

install(TARGETS hypervisor DESTINATION share/NOVA COMPONENT Hedron_Application)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/hypervisor.elf32
  DESTINATION share/NOVA COMPONENT Hedron_Application)

# Ease transition to new name by providing a fallback symlink to the
# old installation path.
#
# This should eventually go away once we updated all dependencies.
install(CODE "execute_process(COMMAND ln -sfT NOVA hedron WORKING_DIRECTORY ${CMAKE_INSTALL_PREFIX}/share)")
