import os
import sys
import struct
import time
import subprocess
from threading import Thread
from twisted.internet import protocol, reactor

# CONFIGURATION CONSTANTS
MONITORS_WIDE = 2
MONITORS_HIGH = 2

DELAY = 3 # In seconds


def get_video_dimenstions(filename):
    proc = subprocess.Popen(
        ('ffprobe -v error -show_entries stream=width,height -of default=noprint_wrappers=1 %s'
         % filename).split(), stdout=subprocess.PIPE)
    output = proc.communicate()[0].split('\n')
    width = int(output[0].split('=')[1])
    height = int(output[1].split('=')[1])

    return width, height


def crop_video(source_filename, dest_filename, width, height, x, y):
    comd = ('ffmpeg -i %s -filter:v crop=%s:%s:%s:%s -c:a copy %s'
        % (source_filename, width, height, x, y, dest_filename))

    print comd
    proc = subprocess.Popen(
        comd.split()
    )
    proc.communicate()

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

        width, height = get_video_dimenstions(filename)
        relative_width = width / MONITORS_WIDE
        relative_height = height / MONITORS_HIGH

        for index, client in enumerate(self.clients):
            x = relative_width * (index % MONITORS_WIDE)
            y = relative_height * (index % MONITORS_HIGH)

            new_video = "/tmp/%s.mp4" % index
            crop_video(filename, new_video, relative_width, relative_height, x, y)
            videos.append(open(new_video))
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
