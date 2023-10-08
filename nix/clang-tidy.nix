{ clang-tools, hedron }:
hedron.overrideAttrs (
  old: {
    name = "hedron-clang-tidy";
    cmakeFlags = old.cmakeFlags or [] ++ [
      "-DENABLE_CLANG_TIDY=ON"
      "-DBUILD_TESTING=ON"
    ];
    nativeBuildInputs = old.nativeBuildInputs or [] ++ [ clang-tools ];
  }
)
