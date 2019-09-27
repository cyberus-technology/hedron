{
  stdenv,
  catch2,
  cmake,
  nix-gitignore,
  lib,
  clang-tools-wrapper ? null
}:

let
  gitIgnores = lib.optional (builtins.pathExists ../.gitignore) ../.gitignore;
in
stdenv.mkDerivation {
  name = "nova";
  src = nix-gitignore.gitignoreSourcePure (["nix\n"] ++ gitIgnores) ./..;

  nativeBuildInputs = [ cmake ] ++ (lib.optional (builtins.isNull clang-tools-wrapper) clang-tools-wrapper);
  buildInputs = [ catch2 ];

  hardeningDisable = [ "all" ];
  enableParallelBuilding = true;
  doCheck = true;

  installPhase = ''
    mkdir -p $out/nix-support $out/share
    cp src/hypervisor src/hypervisor.elf32 $out/
    cp -r $src/doc $out/share
    echo "file binary-dist $out/hypervisor" >> $out/nix-support/hydra-build-products
    echo "file binary-dist $out/hypervisor.elf32" >> $out/nix-support/hydra-build-products
    echo "doc-pdf manual $out/share/doc/specification.pdf" >> $out/nix-support/hydra-build-products
  '';

  meta = with stdenv.lib; {
    description = "NOVA microhypervisor, Cyberus Technology fork";
    homepage = https://gitlab.com/cyberus/NOVA;
    license = licenses.gpl2;
  };
}
