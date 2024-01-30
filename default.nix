with import <nixpkgs> {};
stdenv.mkDerivation {
  name = "tinyauto";
  buildInputs = [
    paho-mqtt-c
    libbsd
  ];
}
