{ stdenv, python3, qemu, netcat }:

stdenv.mkDerivation {
  pname = "boot-qemu";
  version = "0";

  src = ../test/integration;

  buildInputs = [ ];
  propagatedBuildInputs = [ qemu python3.pkgs.pexpect netcat ];

  checkInputs = [ python3.pkgs.black ];

  nativeBuildInputs = [ python3.pkgs.wrapPython ];

  dontConfigure = true;
  dontBuild = true;
  doCheck = true;

  checkPhase = ''
    black --check --diff qemu-boot
  '';

  installPhase = ''
    mkdir -p $out/bin
    install -m 755 qemu-boot $out/bin
  '';

  postFixup = ''
    wrapPythonPrograms
  '';

}
