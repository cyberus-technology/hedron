{
  stdenv,
  catch2,
  cmake,
  nix-gitignore,
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

  nativeBuildInputs = [ cmake ];
  checkInputs = [ catch2 ];

  cmakeBuildType = buildType;
  cmakeFlags = lib.optional (heapSizeMiB != null) "-DHEAP_SIZE_MB=${toString heapSizeMiB}";

  hardeningDisable = [ "all" ];
  enableParallelBuilding = true;
  doCheck = true;

  postInstall = ''
    mkdir -p $out/nix-support $out/share
    cp -r $src/doc $out/share
    echo "file binary-dist $out/share/hedron/hypervisor" >> $out/nix-support/hydra-build-products
    echo "file binary-dist $out/share/hedron/hypervisor.elf32" >> $out/nix-support/hydra-build-products
    echo "doc-pdf manual $out/share/doc/specification.pdf" >> $out/nix-support/hydra-build-products
  '';

  meta = with stdenv.lib; {
    description = "Hedron microhypervisor by Cyberus Technology";
    homepage = "https://github.com/cyberus-technology/hedron";
    license = licenses.gpl2;
  };
}
