/*************************************************************************

Simple serial receiver. Prints anything it receives.

compile using
g++ -g -Wall simple-serial.cpp libraries/Serial/Serial.cpp -o simpl -I..

************************************************************************/

const char* helpText =
"An OSC client to manage Trill devices."
"  Usage: %s [--port <inPort>] [[--auto <bus> ] <remote>]\n"
"  --port <inPort> :  set the port where to listen for OSC messages\n"
"\n"
"  `--auto <bus> <remote> : this is useful for debugging: automatically detect\n"
"                          all the Trill devices on <bus> (corresponding to /dev/i2c-<bus>)\n"
"                          and start reaading from them\n"
"                          All messages read will be sent to the <remote> IP:port (default: %s)\n"
"======================\n"
"\n"
"NOTE: when `--auto` is used, or a `createAll` command is received, the program\n"
"scans several addresses on the i2c bus, which could cause non-Trill\n"
"peripherals connected to it to malfunction.\n"
"\n"
"Command reference:\n"
"Send commands to `/trill/commands/<command>`, with 0 or more arguments.\n"
"\n"
;


#include <libraries/Serial/Serial.h>
#include <cmath>
#include <vector>
#include <string>
#include <memory>
#include <map>
#include <unistd.h>

#define OSCPKT_OSTREAM_OUTPUT
#include "libraries/OSC/oscpkt.hh"
#include <iostream> // needed for udp.hh
#include "libraries/OSC/udp.hh"

#include <signal.h>

Serial gSerial;
int gShouldStop = 0;
int verbose = true;

void interrupt_handler(int var)
{
	gShouldStop = true;
}

oscpkt::UdpSocket gSock;

int parseOsc(oscpkt::Message& msg);
int sendOscFloats(const std::string& address, float* values, unsigned int size);
int sendOscReply(const std::string& command, const std::string& id, int ret);

void readSerial() {
	while(!gShouldStop)
  {
		unsigned int maxLen = 128;
		char serialBuffer[maxLen];
		// read from the serial port with a timeout of 100ms
		int ret = gSerial.read(serialBuffer, maxLen, 100);
		if (ret > 0) {
			printf("%.*s\n", ret, serialBuffer);
			for(int n = 0; n < ret; ++n)
			{
				// when some relevant data is received
        // send an OSC message
				if('k' == serialBuffer[n])
				{
					//gSerial.write("kick!\n\r"); // write serial
          printf("Send OSC k!");
				} else if('s' == serialBuffer[n]) {
          printf("Send OSC s!");
					//gSerial.write("snare!\n\r");
				}
			}
		} else {
			// Nothing read in the serial buffer...
    }
	}
}



int main(int argc, char** argv)
{
	const unsigned int serialBaud = 115200;
  const char* serialPort = "/dev/ttyS5"; // UART5 on the Bela
	//std::string serialPort = "/dev/ttyUSB0";

  const std::string remote = "localhost:57120"; // OSC remote to send messages to.. should be sclang


  // PARSE COMMAND LINE ARGUMENTS
  int c = 1;
  while(c < argc) {
    if(std::string("--help") == std::string(argv[c])) {
      printf(helpText, argv[0], remote.c_str());
      return 0;
    }

    /*
    if(std::string("--port") == std::string(argv[c])) {
      ++c;
      if(c < argc) {
        inPort = atoi(argv[c]);
        if(!inPort) {
          fprintf(stderr, "Invalid port: %s\n", argv[c]);
        }
      }
    }
    if(std::string("--auto") == std::string(argv[c])) {
      ++c;
      if(c < argc) {
        i2cBus = atoi(argv[c]);
        ++c;
      }
      if(c < argc) {
        remote = argv[c];
        ++c;
      }
      std::vector<std::string> spl = split(remote, ':');
      if(2 != spl.size()) {
        fprintf(stderr, "Wrong or unparseable `IP:port` argument: %s\n", remote.c_str());
        return 1;
      }
      gSock.connectTo(spl[0], std::stoi(spl[1]));
      //std::cout << "Detecting all devices on bus " << i2cBus << ", sending to " << remote << "\n(remote address will be reset when the first inbound message is received)\n";
      //gAutoReadAll = true;
      //createAllDevicesOnBus(i2cBus, gAutoReadAll);
    }

    */
    ++c;
  }

  gSerial.setup(serialPort, serialBaud);
	std::cout << "Listening on port " << serialPort << " at " << serialBaud << "\n";

	signal(SIGINT, interrupt_handler); // what's this for? TODO

	//gSock.bindTo(inPort);
	std::string baseAddress = "/serial/";
	std::string address;

  // READ SERIAL DATA
  readSerial();

	return 0;
}


// OSC FUNCTIONS
int sendOsc(const oscpkt::Message& msg)
{
	oscpkt::PacketWriter pw;
	pw.addMessage(msg);
	bool ok = gSock.sendPacket(pw.packetData(), pw.packetSize());
	if(!ok) {
		fprintf(stderr, "could not send\n");
		return -1;
	}
	return 0;
}

int sendOscFloats(const std::string& address, float* values, unsigned int size)
{
	if(!size)
		return 0;
	oscpkt::Message msg(address);
	for(unsigned int n = 0; n < size; ++n)
		msg.pushFloat(values[n]);
	return sendOsc(msg);
}
