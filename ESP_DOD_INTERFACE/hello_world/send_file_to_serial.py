import time
import serial
import os
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('--file', type=str, required=True)
parser.add_argument('--port', type=str, required=True)
args = parser.parse_args()

rec = open("response.txt", "w")

ser = serial.Serial(args.port, 115200)

payload_sent_time = time.time()

def format_bytes(hexstr):
        # Add a space every 2 chars (1 byte)
        return ' '.join([hexstr[j:j+2] for j in range(0, len(hexstr), 2)])

def write_response(response):
    print(response)
    rec.write(response)
    rec.write('\n')
    rec.flush()

def send_line(line):
    global payload_sent_time
    line = format_bytes(line)
    line += '\n'
    print(line)
    ser.write(line.encode('utf-8'))
    ser.flush()
    payload_sent_time = time.time()

def establish_handshake():
    print("Establishing handshake...")
    ser.write("establish_handshake\n".encode('utf-8'))
    ser.flush()
    while True:
        if time.time() - payload_sent_time > 3:
            print("Payload sent time exceeded 3 seconds")
            break
        if ser.in_waiting > 0:
            response = ser.readline().decode('utf-8')
            write_response(response)
            if response.startswith("Establishing handshake... true"):
                break

def send_chunk(chunk, return_on="[USB->UART]") -> bool:
    global payload_sent_time
    send_line(chunk)
    while True:
        if time.time() - payload_sent_time > 3:
            print("Payload sent time exceeded 3 seconds")
            return False
        if ser.in_waiting > 0:
            response = ser.readline().decode('utf-8')
            write_response(response)
            if response.startswith("Error"):
                return False
            if response.startswith(return_on):
                return True


def main():
    establish_handshake()
    with open(args.file, 'r') as f:
        data = f.readlines()
    data_length = len(data)
    for line in data:
        line = line.replace(' ', '').replace('\n', '')
        BYTES = 16
        # INSERT_YOUR_CODE
        # If the line is longer than 16 bytes (32 hex chars), split it into 16-byte (32-char) chunks
        
        if len(line) > BYTES * 2:
            for i in range(0, len(line), BYTES * 2):
                time.sleep(0.03)
                chunk = line[i:i + BYTES * 2]
                if not send_chunk(chunk, "[USB->UART]"):
                    print("Failed to send chunk: ", chunk)
            while True:
                if time.time() - payload_sent_time > 3:
                    print("Payload sent time exceeded 3 seconds")
                    break
                if ser.in_waiting > 0:
                    response = ser.readline().decode('utf-8')
                    write_response(response)
                    if response.startswith("[UART->USB]"):
                        break   
            continue  # skip the rest of the loop for this line, as we've already sent all chunk

        else:
            time.sleep(0.03)
            if not send_chunk(line, "[UART->USB]"):
                print("Failed to send chunk: ", line)
                # send_chunk(line)

        # if len(line) % 8 != 0:
        #     # send the line in chunks of 4 bytes
        #     for i in range(0, len(line), 8):
        #         time.sleep(0.3)

        #         out = line[i:i+2] +" "+ line[i+2:i+4] +" "+ line[i+4:i+6] +" "+ line[i+6:i+8]
        #         ser.write(out.encode())
        #         ser.write('\n'.encode())
        #         ser.flush()
        #         print(out)
                # time.sleep(0.1)
                # wait for response
                # while True:
                #     if ser.in_waiting > 0:
                #         response = ser.readline().decode('utf-8')
                #         if response.startswith("[UART->USB]"):
                #             continue
                #         print(response)
                #         break


    ser.close()

if __name__ == "__main__":
    main()