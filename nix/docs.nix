{ lib
, stdenv
, drawio-headless
, python3
, python3Packages
}:
stdenv.mkDerivation {
  name = "hedron-docs";

  src = lib.sourceByRegex ../. [
    "mkdocs.yml"
    "^docs.*"
  ];

  propagatedBuildInputs = [ drawio-headless ];

  nativeBuildInputs = [
    python3
  ] ++ (with python3Packages; [
    mkdocs
    mkdocs-material
    mkdocs-drawio-exporter
  ]);

  dontConfigure = true;

  buildPhase = ''
    mkdocs build
  '';

  installPhase = ''
    cp -a site/ $out/
  '';
}
