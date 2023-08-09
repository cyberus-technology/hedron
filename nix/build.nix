{
  stdenv,
  catch2,
  cmake,
  nix-gitignore,
  python3,
  lib,
  buildType ? "Debug",
  # Specify a kernel heap size in MiB. This overrides the default and
  # is advisable for more sophisticated workloads.
  heapSizeMiB ? null
}:

let
  gitIgnores = lib.optional (builtins.pathExists ../.gitignore) ../.gitignore;
in
stdenv.mkDerivation {
  name = "hedron";
  src = nix-gitignore.gitignoreSourcePure ([".git\nnix\n"] ++ gitIgnores) ./..;

  nativeBuildInputs = [
    cmake

    # For tools/check-elf-segments.
    (python3.withPackages(ps: [ ps.pyelftools ]))
  ];

  checkInputs = [ catch2 ];

  cmakeBuildType = buildType;
  cmakeFlags = [ "-DENABLE_ELF_SEGMENT_CHECKS:bool=ON" ]
               ++ lib.optional (heapSizeMiB != null) "-DHEAP_SIZE_MB=${toString heapSizeMiB}";

  hardeningDisable = [ "all" ];
  enableParallelBuilding = true;
  doCheck = true;

  postPatch = ''
    patchShebangs tools/check-elf-segments
  '';

  meta = {
    description = "Hedron hypervisor by Cyberus Technology";
    homepage = "https://github.com/cyberus-technology/hedron";
    license = lib.licenses.gpl2;
    platforms = [ "x86_64-linux" ];
  };
}
