import os
import sys
import struct
import time
from threading import Thread
from twisted.internet import protocol, reactor


DELAY = 3 # In seconds

class MultiScreenProtocol(protocol.BaseProtocol):

    ENUM = {
        'UPLOAD': 0,
        'INIT': 1,
        'PLAY': 2,
        'PAUSE': 3,
        'TIME': 4
    }


    def connectionMade(self):
        self.factory.clients.append(self)

    def connectionLost(self, reason):
        self.factory.clients.remove(self)

    def dataRecieved(self, data):
        pass


class MultiScreenFactory(protocol.ServerFactory):
    protocol = MultiScreenProtocol
    clients = list()
    sequence = 0

    def getClientVideos(self, filename):
        filename = os.path.expanduser(filename);
        videos = list()
        for client in self.clients:
            videos.append(open(filename))
        return videos

    def sendCommand(self, command):
        """
        The server's protocol is incredibly simple and is as follows:
        * Send a sequence number, an unsigned long
        * Send an unsigned char representing the base command (according to the ENUM dict)
        * If the command is PLAY or PAUSE then the server will then send an unsigned int with the unix time to execute the command
        * If the command is UPLOAD the server will send an unsigned long with the length, in bytes, of the upload
        * The server will then send the data 
        """

        videos = list()
        filename = command[1] if len(command) == 2 else None
        if command[0] == 'UPLOAD' and filename:
            videos = self.getClientVideos(filename)

        for index, c in enumerate(self.clients):
            # Send Sequence number
            c_sequence = struct.pack('L', self.sequence)
            c.transport.write(c_sequence)

            # Send Command
            c_command = struct.pack('B', MultiScreenProtocol.ENUM.get(command[0]))
            c.transport.write(c_command)


            if command[0] in ["PLAY", "PAUSE"]:
                delay = int(time.time()) + DELAY
                c_delay = struct.pack('I', delay)
                c.transport.write(c_delay)

            # If we're doing an upload
            elif videos:
                f = videos[index]

                # Send file size
                size = os.fstat(f.fileno()).st_size
                c_size = struct.pack('L', size)
                print "Sending file"
                c.transport.write(c_size)

                # Send the file contents
                f.seek(0)
                while True:
                    chunk = f.read(8192)
                    if chunk:
                        c.transport.write(chunk)
                    else:
                        break
                f.close()

        self.sequence += 1


def main_loop(factory):
    while True:
        command  = raw_input("Command: >> ").split(" ")
        command[0] = command[0].upper()

        if command[0] in MultiScreenProtocol.ENUM.keys():
            factory.sendCommand(command)

        elif command[0]  in ['Q', 'QUIT']:
            import thread
            thread.interrupt_main()
            break
        else:
            print "Usage:\n" + \
                  "\tUPLOAD [filename]: Trigger upload of derivative videos on all nodes\n" + \
                  "\tINIT: Initialize the video for play" + \
                  "\tPLAY: Start video on all nodes\n" + \
                  "\tPAUSE: Pause video on all nodes"

if __name__ == "__main__":
    factory = MultiScreenFactory()
    reactor.listenTCP(8000, factory)
    Thread(target=main_loop, args=(factory,)).start()
    reactor.run()
