# AngryUEFI

An UEFI application that can perform microcode updates as instructed via network.

## Repository Structure

### AngryUEFI

This repo. This is the actual UEFI application, written mostly in C. It gets placed in a subfolder (git submodule) of the AngryEDK2 repo. The actual build system is in the AngryEDK2 repo.

*Note* The submodule in the AngryEDK2 repo is not neccessarily updated. Do a git pull in this submodule to get the most recent changes.

### AngryEDK2

A fork of the official EDK2 [repo](https://github.com/tianocore/edk2). Contains the build system and the AngryUEFI application as a submodule.

### AngryCAT

A python application that drives the microcode testing. It talks to AngryUEFI via network sockets.

## Network Protocol

Documented in the AngryCAT repo. tl;dr: LMTV: 4 byte unsigned BE Length | 4 byte Metadata | 4 byte Type | Payload

## Building

Documented in the AngryEDK2 repo. tl;dr: cd AngryEDK2; ./init.sh; ./build.sh; ./run.sh

## License
[BSD License](http://opensource.org/licenses/bsd-license.php).

Thanks to [TcpTransport](https://github.com/vinxue/TcpTransport) for pointers on the "network stack"!
