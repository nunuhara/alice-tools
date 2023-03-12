{
  description = "Tools for extracting/editing files from AliceSoft games";

  # Nixpkgs / NixOS version to use.
  inputs.nixpkgs.url = "nixpkgs/nixos-22.11";

  outputs = { self, nixpkgs }:
    let

      # System types to support.
      supportedSystems = [ "x86_64-linux" "x86_64-darwin" "aarch64-linux" "aarch64-darwin" ];

      # Helper function to generate an attrset '{ x86_64-linux = f "x86_64-linux"; ... }'.
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;

      # Nixpkgs instantiated for supported system types.
      nixpkgsFor = forAllSystems (system: import nixpkgs { inherit system; });

    in {

      # Set up alice-tools development environment:
      #     nix develop # create shell environment with dependencies available
      devShell = forAllSystems (system:
        let pkgs = nixpkgsFor.${system}; in with pkgs; pkgs.mkShell {
          nativeBuildInputs = [ meson ninja pkg-config ];
          buildInputs = [
            libsForQt5.qtbase
            libpng
            libjpeg
            libwebp
            flex
            bison
          ];
        }
      );

      # Build alice-tools:
      #     nix build .?submodules=1 # build alice-tools (outputs to ./result/)
      #     nix shell .?submodules=1 # create shell environment with alice-tools available
      # Install alice-tools to user profile:
      #     nix profile install .?submodules=1
      packages = forAllSystems (system:
        let pkgs = nixpkgsFor.${system}; in {
          default = with pkgs; stdenv.mkDerivation rec {
            name = "alice";
            src = ./.;
            mesonAutoFeatures = "auto";
            nativeBuildInputs = [ meson ninja pkg-config libsForQt5.qt5.wrapQtAppsHook ];
            buildInputs = [
              libsForQt5.qtbase
              libpng
              libjpeg
              libwebp
              flex
              bison
            ];
          };
        }
      );
    };
}
