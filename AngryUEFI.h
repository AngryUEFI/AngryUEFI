#ifndef ANGRYUEIF_H
#define ANGRYUEIF_H

#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/PrintLib.h>
#include <Protocol/SimpleTextOut.h>
#include <Uefi.h>

// TODO: Figure out how to pass this in via the build system
// Note: you need to flip between build modes passed to ./build.sh for this get picked up
// e.g. if currently in DEBUG, build once as RELEASE, then again as DEBUG
// no, this does not make any sense
// #define ANGRYUEFI_DEBUG

#ifdef ANGRYUEFI_DEBUG
#define PrintDebug(text) TextOutput->OutputString(TextOutput, text)
#define FormatPrintDebug(fmt, ...) \
    do { UnicodeSPrint(FormatBuffer, sizeof(FormatBuffer), fmt, ##__VA_ARGS__);  PrintDebug(FormatBuffer);} while (0)
#else
#define PrintDebug(text)
#define FormatPrintDebug(fmt, ...)
#endif

#define Print(text) TextOutput->OutputString(TextOutput, text)
#define FormatPrint(fmt, ...) \
    do { UnicodeSPrint(FormatBuffer, sizeof(FormatBuffer), fmt, ##__VA_ARGS__);  Print(FormatBuffer);} while (0)

#define RECEIVE_BUFFER_SIZE 8192+12
extern UINT8 receive_buffer[RECEIVE_BUFFER_SIZE];
extern EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *TextOutput;

extern UINT16 FormatBuffer[128];

typedef struct ConnectionContext_s ConnectionContext;

EFI_STATUS send_message(void* message, UINTN message_size, ConnectionContext* ctx);
VOID EFIAPI DummyNotiftyFunction(IN EFI_EVENT Event, IN VOID *Context);

#endif /* ANGRYUEIF_H */
