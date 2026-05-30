{
  description = "aligner — a C source formatter that aligns every line's symbols to column 80";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    ds = {
      url = "github:hydrastro/ds";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, ds }:
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

            # Pass the ds flake input's source path through to make as DSDIR.
            # The Makefile defaults to ./ds for local checkouts; here we point
            # it at the nix store so the source tree stays free of ds/.
            buildPhase = ''
              make -j$NIX_BUILD_CORES DSDIR=${ds}
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

          # Self-hosted: just compile the committed aligner.c with cc + libc.
          # No ds dependency, no Makefile. The source is its own demonstration.
          aligner-self = pkgs.stdenv.mkDerivation {
            pname = "aligner-self";
            version = "0.1.0";
            src = ./.;
            nativeBuildInputs = [ pkgs.gcc ];
            buildPhase = ''
              cc -std=c99 -O2 -Wall -Wextra -Wpedantic -o aligner-self aligner.c
            '';
            installPhase = ''
              install -Dm755 aligner-self $out/bin/aligner
            '';
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

            # Drop ds's source into the shell as $DSDIR so plain `make`
            # works without a local clone or symlink.
            shellHook = ''
              export DSDIR="${ds}"
              echo "aligner dev shell"
              echo "  DSDIR=$DSDIR  (from the ds flake input)"
            '';
          };
        });
    };
}
