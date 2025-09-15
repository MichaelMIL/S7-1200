class SerialMock:
    def __init__(self, is_debug=False):
        self.text_file = open("exploit.txt", "w")
        self.debug = is_debug

    def write(self, data):
        self.text_file.write(data)
        self.text_file.write("\n")
        self.text_file.flush()


    def write_debug(self, data):
        if self.debug:
            self.write(data)

    def send(self, data, timeout=None, is_str=False):
        # data = bytes(data, "latin-1")
        if not is_str:
            raw_data = data.encode("latin-1").hex()
            data = ""
            length = len(raw_data) 
            if length%2 == 0:
                for i in range(0, length, 2):
                    data += raw_data[i:i+2]
                    data += " "
        self.write(data.upper())
        print(f"Sending data: {data.upper()}")
    def recv(self, data, timeout=None):
        print(f"Receiving data: {data}")

    def close(self):
        print("Closing serial connection")