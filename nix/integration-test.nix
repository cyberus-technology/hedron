{ hedron, grub2, mtools, OVMF, xorriso, stdenv, qemuBoot }:
let
  grub_image = "grub_image.iso";
in
stdenv.mkDerivation {
  name = "hedron-integration-tests";
  inherit (hedron) src;

  nativeBuildInputs = [ grub2 mtools OVMF xorriso qemuBoot ];

  postPatch = ''
    patchShebangs tools/gen_usb.sh
  '';

  buildPhase = ''
    qemu-boot ${hedron}/share/hedron/hypervisor.elf32 | tee output.log
    tools/gen_usb.sh ${grub_image} ${hedron}/share/hedron/hypervisor.elf32 tools/grub.cfg.tmpl
    qemu-boot ${grub_image} --disk-image | tee -a output.log
    qemu-boot ${grub_image} --disk-image --uefi --uefi-firmware-path ${OVMF.fd}/FV | tee -a output.log
  '';

  installPhase = ''
    mkdir -p $out/nix-support
    cp output.log $out/
    echo "report testlog $out output.log" > $out/nix-support/hydra-build-products
  '';
}
