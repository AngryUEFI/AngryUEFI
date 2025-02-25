#include <Base.h>
#include <Library/BaseLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/DebugLib.h>
#include <Library/PrintLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/ShellLib.h>
#include <Library/TimerLib.h>
#include <Library/NetLib.h>
#include <Protocol/SimpleTextOut.h>
#include <Protocol/SimpleTextIn.h>
#include <Protocol/Dhcp4.h>
#include <Protocol/Tcp4.h>
#include <Protocol/ServiceBinding.h>

#include "Protocol.h"
#include "AngryUEFI.h"
#include "stubs.h"

UINT8 receive_buffer[RECEIVE_BUFFER_SIZE];
EFI_SIMPLE_TEXT_OUTPUT_PROTOCOL *TextOutput;

struct ConnectionContext_s {
    EFI_TCP4_PROTOCOL* TCPConnection;
};

UINT16 FormatBuffer[128] = {0};

VOID EFIAPI DummyNotiftyFunction(IN EFI_EVENT Event, IN VOID *Context) {
    PrintDebug(L"Notified.\n");
    return;
}

EFI_STATUS send_message(void* message, UINTN message_size, ConnectionContext* ctx) {
    EFI_STATUS Status;
    EFI_TCP4_IO_TOKEN TxToken = {0};
    EFI_TCP4_TRANSMIT_DATA TxData = {0};

    EFI_TCP4_PROTOCOL* IncomingTcp4 = ctx->TCPConnection;

    TxData.DataLength = message_size;
    TxData.FragmentCount = 1;
    TxData.FragmentTable[0].FragmentBuffer = message;
    TxData.FragmentTable[0].FragmentLength = message_size;
    TxToken.Packet.TxData = &TxData;

    Status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL,
                            TPL_CALLBACK,
                            DummyNotiftyFunction,
                            NULL,
                            &TxToken.CompletionToken.Event);
    if (EFI_ERROR(Status)) {
        FormatPrintDebug(L"Error: Could not create event for TxToken: %r\n", Status);
        return Status;
    }
    TxToken.CompletionToken.Status = EFI_NOT_READY;
    PrintDebug(L"Got TxToken event.\n");

    Status = IncomingTcp4->Transmit(IncomingTcp4, &TxToken);
    if (EFI_ERROR(Status)) {
        FormatPrintDebug(L"Error: TCP Transmit failed: %r\n", Status);
        gBS->CloseEvent(TxToken.CompletionToken.Event);
        return Status;
    }

    // Wait until the transmit completes.
    while (TxToken.CompletionToken.Status == EFI_NOT_READY) {
        gBS->Stall(100000); // 100ms
    }
    if (EFI_ERROR(TxToken.CompletionToken.Status)) {
        FormatPrintDebug(L"Error: TCP Transmit completed with error: %r\n", TxToken.CompletionToken.Status);
        gBS->CloseEvent(TxToken.CompletionToken.Event);
        return TxToken.CompletionToken.Status;
    }
    PrintDebug(L"Transmitted.\n");

    return EFI_SUCCESS;
}

static EFI_STATUS receive_chunk(void* chunk, UINTN chunk_capacity, UINTN* chunk_length, EFI_TCP4_PROTOCOL* IncomingTcp4) {
    EFI_STATUS Status;

    // by default indicate no message received
    *chunk_length = 0;

    EFI_TCP4_IO_TOKEN RxToken = {0};
    EFI_TCP4_RECEIVE_DATA  RxData = {0};

    RxData.DataLength    = chunk_capacity;
    RxData.FragmentCount = 1;
    RxData.FragmentTable[0].FragmentBuffer = chunk;
    RxData.FragmentTable[0].FragmentLength = chunk_capacity;
    RxToken.Packet.RxData = &RxData;

    RxToken.CompletionToken.Status = EFI_NOT_READY;
    Status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL,
                            TPL_CALLBACK,
                            DummyNotiftyFunction,
                            NULL,
                            &RxToken.CompletionToken.Event);
    if (EFI_ERROR(Status)) {
        FormatPrintDebug(L"Error: Could not create event for RxToken: %r\n", Status);
        return Status;
    }
    PrintDebug(L"Got RxToken event.\n");

    Status = IncomingTcp4->Receive(IncomingTcp4, &RxToken);
    if (Status == 0x8000000000000068uLL) {
        // this likely indicates the peer closed the connection
        // unknown why this code indicates this
        gBS->CloseEvent(RxToken.CompletionToken.Event);
        PrintDebug(L"Peer closed the conneciton\n");
        *chunk_length = 0;
        return EFI_SUCCESS;
    } else if (EFI_ERROR(Status)) {
        FormatPrintDebug(L"Error: TCP Receive failed: %r\n", Status);            
        gBS->CloseEvent(RxToken.CompletionToken.Event);
        return Status;
    }
    PrintDebug(L"Receiving.\n");

    // Wait until the receive completes.
    while (RxToken.CompletionToken.Status == EFI_NOT_READY) {
        gBS->Stall(100000); // 100ms
    }

    if (RxToken.CompletionToken.Status == 0x8000000000000068uLL) {
        // this likely indicates the peer closed the connection
        // unknown why this code indicates this
        gBS->CloseEvent(RxToken.CompletionToken.Event);
        PrintDebug(L"Peer closed the conneciton\n");
        *chunk_length = 0;
        return EFI_SUCCESS;
    } else if (EFI_ERROR(RxToken.CompletionToken.Status)) {
        FormatPrintDebug(L"Error: TCP Receive completed with error: %r, %u\n", RxToken.CompletionToken.Status, RxToken.CompletionToken.Status);
        gBS->CloseEvent(RxToken.CompletionToken.Event);
        return RxToken.CompletionToken.Status;
    }
    *chunk_length = RxToken.Packet.RxData->DataLength;
    if (*chunk_length == 0) {
        FormatPrintDebug(L"Connection closed by peer.\n");
        gBS->CloseEvent(RxToken.CompletionToken.Event);
        *chunk_length = 0;
        return EFI_SUCCESS;
    }
    FormatPrintDebug(L"Received %u bytes chunk.\n", *chunk_length);

    // Clean up per-iteration resources.
    gBS->CloseEvent(RxToken.CompletionToken.Event);

    return EFI_SUCCESS;
}

static EFI_STATUS receive_messages(EFI_TCP4_PROTOCOL* IncomingTcp4) {
    EFI_STATUS Status;

    // by default indicate no message received
    UINTN message_length = 0;
    ConnectionContext ctx = {0};
    ctx.TCPConnection = IncomingTcp4;
    for (;;) {
        // High level overview
        // 1. Receive 4 Bytes to get message length
        // 2. Receive Remainder of message
        // 2.1. loop receive calls until message length reached
        // 3. Dispatch to message handling
        // 4. Loop for next message
        
        UINTN header_message_length = 0;
        UINTN current_message_size = 0;
        Status = receive_chunk(receive_buffer, 4, &message_length, IncomingTcp4);
        if (EFI_ERROR(Status)) {
            FormatPrintDebug(L"Unable to receive chunk: %r\n", Status);
            return Status;
        }
        if (message_length == 0) {
            return EFI_SUCCESS;
        }
        if (message_length != 4) {
            FormatPrintDebug(L"Unable to read message length, read length: %u.\n", message_length);
            return EFI_ABORTED;
        }
        header_message_length = *(UINT32*)receive_buffer;
        FormatPrintDebug(L"Header indicates %u Bytes message.\n", header_message_length);
        while (current_message_size < header_message_length) {
            Status = receive_chunk(receive_buffer + current_message_size, header_message_length - current_message_size, &message_length, IncomingTcp4);
            if (EFI_ERROR(Status)) {
                FormatPrintDebug(L"Unable to receive chunk: %r\n", Status);
                return Status;
            }
            if (message_length == 0) {
                break;
            }
            current_message_size += message_length;
            FormatPrintDebug(L"Currently have %u Bytes.\n", current_message_size);
        }
        
        if (current_message_size != header_message_length) {
            FormatPrintDebug(L"Expected to read %u Bytes, only got %u Bytes.\n", header_message_length, current_message_size);
            return EFI_ABORTED;
        }

        FormatPrintDebug(L"Read %u Bytes message.\n", current_message_size);

        Status = handle_message(receive_buffer, current_message_size, &ctx);
        if (EFI_ERROR(Status)) {
            FormatPrintDebug(L"Unable to handle message: %r\n", Status);
            return Status;
        }

        PrintDebug(L"Handled message.\n");
    }
    return EFI_SUCCESS;
}

EFI_STATUS static
TcpEchoServer(
  IN EFI_HANDLE           ImageHandle,
  IN EFI_SYSTEM_TABLE     *SystemTable
) {
    EFI_STATUS                          Status;
    UINTN                               HandleCount;
    EFI_HANDLE                          *HandleBuffer;
    EFI_SERVICE_BINDING_PROTOCOL        *Tcp4ServiceBinding;
    EFI_HANDLE                          Tcp4ChildHandle = NULL;
    EFI_IP4_MODE_DATA                   Ip4ModeData;
    EFI_TCP4_PROTOCOL                   *Tcp4 = NULL;
    EFI_TCP4_PROTOCOL                   *IncomingTcp4 = NULL;
    EFI_TCP4_CONFIG_DATA                Tcp4ConfigData;
    EFI_TCP4_LISTEN_TOKEN               ListenToken;
    EFI_TCP4_CLOSE_TOKEN                CloseToken;

    Status = gBS->LocateProtocol (&gEfiSimpleTextOutProtocolGuid, NULL, (VOID**)&TextOutput);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Couldn't open Text Output Protocol: %r\n", Status);
        return Status;
    }

    Print(L"Hello UEFI!\n");
    FormatPrintDebug(L"Hello %u Format!\n", 42);

    UINT64 ret = test_stub(0x123456789ABCDEF0ull, 0x0FEDCBA987654321ull);
    UINT64 expected = (0x123456789ABCDEF0ull + 0x0FEDCBA987654321ull) ^ 0xdeadbeefdeadc0deull;
    if (ret != expected) {
        FormatPrint(L"test stub returned invalid. got 0x%016llX, expected 0x%016llX.\n", ret, expected);
    }
    
    // Locate handles that support the TCP4 Service Binding Protocol.
    Status = gBS->LocateHandleBuffer(ByProtocol,
                                     &gEfiTcp4ServiceBindingProtocolGuid,
                                     NULL,
                                     &HandleCount,
                                     &HandleBuffer);
    if (EFI_ERROR(Status) || HandleCount == 0) {
        FormatPrint(L"Error: Could not locate TCP4 Service Binding handles: %r\n", Status);
        return Status;
    }

    Print(L"Got TCP4 Service Binding Handle.\n");

    // For simplicity, use the first available handle.
    Status = gBS->OpenProtocol(HandleBuffer[0],
                                 &gEfiTcp4ServiceBindingProtocolGuid,
                                 (VOID**)&Tcp4ServiceBinding,
                                 ImageHandle,
                                 NULL,
                                 EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Error: Could not get TCP4 Service Binding interface: %r\n", Status);
        goto CLEANUP;
    }

    Print(L"Got TCP4 Service Binding Interface.\n");

    // Create a child instance for the TCP4 protocol.
    Status = Tcp4ServiceBinding->CreateChild(Tcp4ServiceBinding, &Tcp4ChildHandle);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Error: Could not create TCP4 child: %r\n", Status);
        goto CLEANUP;
    }
    Print(L"Got TCP4 child.\n");

    Status = gBS->OpenProtocol(Tcp4ChildHandle,
                                 &gEfiTcp4ProtocolGuid,
                                 (VOID **)&Tcp4,
                                 ImageHandle,
                                 NULL,
                                 EFI_OPEN_PROTOCOL_GET_PROTOCOL);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Error: Could not get TCP4 protocol interface: %r\n", Status);
        goto CLEANUP;
    }
    Print(L"Got TCP4 protocol interface.\n");

    // Zero and initialize the configuration data.
    ZeroMem(&Tcp4ConfigData, sizeof(Tcp4ConfigData));
    Tcp4ConfigData.AccessPoint.UseDefaultAddress = TRUE;
    Tcp4ConfigData.AccessPoint.StationPort       = 3239;
    Tcp4ConfigData.AccessPoint.ActiveFlag        = FALSE;
    Tcp4ConfigData.AccessPoint.RemotePort        = 0;
    ZeroMem(&Tcp4ConfigData.AccessPoint.RemoteAddress, sizeof(Tcp4ConfigData.AccessPoint.RemoteAddress));
    Tcp4ConfigData.TypeOfService                   = 0;
    Tcp4ConfigData.TimeToLive                      = 64;

    // Prepare a listen token to wait for an incoming connection.
    Status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL,
                              TPL_CALLBACK,
                              DummyNotiftyFunction,
                              NULL,
                              &ListenToken.CompletionToken.Event);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Error: Could not create event for ListenToken: %r\n", Status);
        goto CLEANUP;
    }
    Print(L"Got ListenToken event.\n");

    // Prepare a close token to signal connection closure.
    Status = gBS->CreateEvent(EVT_NOTIFY_SIGNAL,
                              TPL_CALLBACK,
                              DummyNotiftyFunction,
                              NULL,
                              &CloseToken.CompletionToken.Event);
    if (EFI_ERROR(Status)) {
        FormatPrint(L"Error: Could not create event for CloseToken: %r\n", Status);
        goto CLEANUP;
    }
    Print(L"Got CloseToken event.\n");

    // Configure the TCP instance.
    Status = Tcp4->Configure(Tcp4, &Tcp4ConfigData);
    if (Status == EFI_NO_MAPPING) {
        // wait until network is up, e.g. DHCP is configured
        do {
            Status = Tcp4->GetModeData(Tcp4,
                                     NULL, NULL,
                                     &Ip4ModeData,
                                     NULL, NULL
                                     );
            ASSERT_EFI_ERROR(Status);
        } while (!Ip4ModeData.IsConfigured && Ip4ModeData.IsStarted);
        Status = Tcp4->Configure(Tcp4, &Tcp4ConfigData);
    }
    else if (EFI_ERROR(Status)) {
        FormatPrint(L"Error: TCP Configure failed: %r\n", Status);
        goto CLEANUP;
    }
    Print(L"TCP Configured.\n");

    // At this point, a connection is accepted. Enter the echo loop.
    for (;;) {
        PrintDebug(L"Looping for new connection.\n");

        // Start listening for incoming connections.
        ListenToken.CompletionToken.Status = EFI_NOT_READY;
        Status = Tcp4->Accept(Tcp4, &ListenToken);
        if (EFI_ERROR(Status)) {
            FormatPrint(L"Error: TCP Listen failed: %r\n", Status);
            goto CLEANUP;
        }

        PrintDebug(L"TCP Echo Server is listening on port 3239...\n");

        // Wait (busy-wait loop) until a connection is accepted.
        while (ListenToken.CompletionToken.Status == EFI_NOT_READY) {
            gBS->Stall(100000); // stall 100ms
        }
        if (EFI_ERROR(ListenToken.CompletionToken.Status)) {
            FormatPrint(L"Error: Listen token completed with error: %r\n", ListenToken.CompletionToken.Status);
            goto CLEANUP;
        }
        PrintDebug(L"Got incoming connection.\n");

        // An incoming TCP connection gets a new instance of the TCP protocol
        Status = gBS->OpenProtocol(ListenToken.NewChildHandle,
                                &gEfiTcp4ProtocolGuid,
                                (VOID **)&IncomingTcp4,
                                ImageHandle,
                                NULL,
                                EFI_OPEN_PROTOCOL_GET_PROTOCOL);
        if (EFI_ERROR(Status)) {
            FormatPrint(L"Error: Could not get incoming TCP4 protocol interface: %r\n", Status);
            goto CLEANUP;
        }
        PrintDebug(L"Got incoming TCP4 protocol interface.\n");

        Status = receive_messages(IncomingTcp4);
         if (EFI_ERROR(Status)) {
            FormatPrint(L"Unable to receive message: %r\n", Status);
            goto CLEANUP;
        }
    }

CLEANUP:
    // Close the TCP connection and destroy the child instance.
    if (Tcp4 != NULL) {
        Tcp4->Close(Tcp4, NULL);
    }
    if (Tcp4ServiceBinding != NULL && Tcp4ChildHandle != NULL) {
        Tcp4ServiceBinding->DestroyChild(Tcp4ServiceBinding, Tcp4ChildHandle);
    }
    if (HandleBuffer != NULL) {
        FreePool(HandleBuffer);
    }
    return EFI_SUCCESS;
}

EFI_STATUS EFIAPI UefiMain
(
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
) {
    return TcpEchoServer(ImageHandle, SystemTable);
}
