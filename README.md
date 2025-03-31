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

## Job Handling in AngryUEFI

AngryUEFI processes SMP supported jobs, e.g. APPLYUCODEEXCUTETEST, on a per core basis. Core 0 is the boot core and will block until the job returns control back to AngryUEFI, other cores (APs) allow asynchronous execution. APs busy wait until a new job is ready for them.

Once an AP is stared, it waits for jobs to come in from core 0. Core 0 fills the control structures with the data from, e.g. APPLYUCODEEXCUTETEST, and sets the ready bit in the structure. The AP will then execute the job and write back results and state into the control structure. Core 0 waits for the given timeout or job completion and sends back the current state and a flag to indicate whether the timeout was reached or the job completed on its own.

From AngryCAT use GETCORESTATUS and GETLASTTESTRESULT to get the job status on a core. AngryUEFI does not keep track of jobs itself, it only dispatches them to cores. AngryCAT is responsible for keeping a mapping between cores and jobs if required. GETLASTTESTRESULT will always return the current state, even if the job is not done or the core it is running on hangs. This might return corrupt or incomplete results, treat it with caution. Check the flags in the CORESTATUSRESPONSE for the job status.

AngryUEFI will not notify AngryCAT of a finished job, it only returns the status when polled.

A timeout = 0 means synchronous execution, even on APs. Core 0 will wait forever for the job to complete and then send a UCODEEXECUTETESTRESPONSE packet. If the job endless loops or hangs the AP, AngryUEFI will stop responding to packets and require a hard system reset via external means, e.g. system pins.

Remember to start APs via STARTCORE, AngryUEFI will reject jobs for cores that are not running (or not present). After a reboot all cores, expect core 0, are stopped. AngryUEFI also rejects jobs for cores that are otherwise not ready to accept them, e.g. currently running a job or locked up.

### Example flow for running an async job on core 1
1. AngryCAT sends APPLYUCODEEXCUTETEST with core = 1 and timeout = 1
2. Core 0 receives request and fills data structure
3. Core 0 enters wait loop based on timeout
4. Core 1 starts executing job
5. Core 0 reaches timeout
6. Core 0 sends back UCODEEXECUTETESTRESPONSE with timeout reached = 1
7. Core 1 finishes executing job, enters wait loop for new job
8. AngryCAT sends GETLASTTESTRESULT with core = 1
9. Core 0 receives request
10. Core 0 sends CORESTATUSRESPONSE with ready = 1
11. Core 0 sends UCODEEXECUTETESTRESPONSE with state from core 1

Between 6. and 7. AngryCAT can optionally poll job status via GETCORESTATUS to wait for ready = 1

## License
[BSD License](http://opensource.org/licenses/bsd-license.php).

Thanks to [TcpTransport](https://github.com/vinxue/TcpTransport) for pointers on the "network stack"!
