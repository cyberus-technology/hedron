{ expect, nova, qemu, stdenv }:
stdenv.mkDerivation {
  name = "nova-integration-test";
  inherit (nova) src;

  nativeBuildInputs = [ expect qemu ];

  postPatch = ''
    patchShebangs test/integration/qemu-boot
  '';

  buildPhase = ''
    test/integration/qemu-boot ${nova}/hypervisor.elf32 | tee output.log
  '';

  installPhase = ''
    mkdir -p $out/nix-support
    cp output.log $out/
    echo "report testlog $out output.log" > $out/nix-support/hydra-build-products
  '';
}
