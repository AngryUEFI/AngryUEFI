#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <Base.h>

#include "AngryUEFI.h"

EFI_STATUS handle_message(UINT8* message, UINTN message_length, ConnectionContext* ctx);
EFI_STATUS construct_message(UINT8* message_buffer, UINTN buffer_capacity, UINT32 message_type, UINT8* payload, UINTN payload_length, BOOLEAN last_message);
EFI_STATUS send_status(UINT32 status_code, CHAR16* message, ConnectionContext* ctx);

#define RESPONSE_PAYLOAD_SIZE 1024 + 32
#define RESPONSE_BUFFER_SIZE RESPONSE_PAYLOAD_SIZE + 12 // 1024 Bytes Payload + 4 Bytes Length + 4 Bytes Metadata + 4 Bytes Message Type
#define HEADER_SIZE 12

extern UINT8 payload_buffer[RESPONSE_PAYLOAD_SIZE];
extern UINT8 response_buffer[RESPONSE_BUFFER_SIZE];

#pragma pack(push, 1)
typedef struct MetaData_s {
    UINT8 Major;
    UINT8 Minor;
    UINT8 Control;
    UINT8 Reserved;
} MetaData;
#pragma pack(pop)

enum MessageType {
    MSG_PING = 0x1,
    MSG_MULTIPING = 0x2,
    MSG_GETMSGSIZE = 0x3,

    MSG_REBOOT = 0x21,

    MSG_SENDUCODE = 0x101,
    MSG_FLIPBITS = 0x121,
    MSG_APPLYUCODE = 0x141,
    MSG_APPLYUCODEEXCUTETEST = 0x151,
    MSG_READMSR = 0x201,
    MSG_GETCORECOUNT = 0x211,
    MSG_SENDMACHINECODE = 0x301,

    MSG_STATUS = 0x80000000,
    MSG_PONG = 0x80000001,
    MSG_MSGSIZE = 0x80000003,

    MSG_UCODERESPONSE = 0x80000141,
    MSG_UCODEEXECUTETESTRESPONSE = 0x80000151,
    MSG_MSRRESPONSE = 0x80000201,
    MSG_CORECOUNTRESPONSE = 0x80000211,
};

#endif /* PROTOCOL_H */
