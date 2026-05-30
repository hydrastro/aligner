{
  description = "aligner — a C source formatter that aligns every line's symbols to column 80";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in {
      packages = forAllSystems (system:
        let pkgs = import nixpkgs { inherit system; };
        in rec {
          aligner = pkgs.stdenv.mkDerivation {
            pname = "aligner";
            version = "0.1.0";
            src = ./.;

            nativeBuildInputs = [ pkgs.gnumake pkgs.gcc ];

            enableParallelBuilding = true;

            buildPhase = ''
              make -j$NIX_BUILD_CORES
            '';

            installPhase = ''
              install -Dm755 build/aligner $out/bin/aligner
            '';

            meta = with pkgs.lib; {
              description = "A C source formatter with a sense of humor about column 80";
              license = licenses.mit;
              platforms = platforms.unix;
            };
          };

          default = aligner;
        });

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.aligner}/bin/aligner";
        };
      });

      devShells = forAllSystems (system:
        let pkgs = import nixpkgs { inherit system; };
        in {
          default = pkgs.mkShell {
            name = "aligner-dev";
            packages = with pkgs; [
              gcc
              gnumake
              gdb
              valgrind
              clang-tools  # clangd for editor support
              bear         # for compile_commands.json
            ];
          };
        });
    };
}
