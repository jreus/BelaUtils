/*************************************************************************

Forwards Incoming Serial Data to SuperCollider via OSC.

Run using:
serial2osc --port /dev/ttyS5 --baud 115200 --remote localhost:57120 --verbose


On the Bela it's best to compile against the on-board Serial library:

g++ -g -Wall serial2osc.cpp ~/Bela/libraries/Serial/Serial.cpp -o serial2osc -I.


Otherwise compile the local copy of Serial.cpp with:

g++ -g -Wall serial2osc.cpp libraries/Serial/Serial.cpp -o serial2osc -I.



************************************************************************/

const char* helpText =
"A utility to convert incoming serial data to OSC.\n"
"  Usage: %s [--port <inPort>] [--baud <baud rate>] [--remote <remote:port>]\n"
"  --port <inPort> :  set the serial port to listen to, uses /dev/ttyS5 by default \n"
"\n"
"  --baud <baud rate> :  serial baud rate, uses 115200 by default \n"
"\n"
"  --remote <remote:port> : remote OSC server:port to send\n"
"              All parsed serial data will be sent to the <remote> IP:port (default: %s)\n"
"\n"
"  --verbose :  flag enables verbose output \n"
"\n"
"  --help :  show this help text \n"

"======================\n"
"\n"
"Command reference:\n"
"Commands will be sent to `/serial`, with 0 or more arguments.\n"
"\n"
;

#include <libraries/Serial/Serial.h>
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
oscpkt::UdpSocket gSock;
int gShouldStop = 0;
int gVerbose = false;

void interrupt_handler(int var) {
	gShouldStop = true;
}

// OSC FUNCTIONS
int sendOsc(const oscpkt::Message& msg)
{
	oscpkt::PacketWriter pw;
	pw.addMessage(msg);
	bool ok = gSock.sendPacket(pw.packetData(), pw.packetSize());
	if(!ok) {
		fprintf(stderr, "Error: could not send OSC message, is destination server alive?\n");
		return -1;
	}
	return 0;
}

// 1.3 of https://www.fluentcpp.com/2017/04/21/how-to-split-a-string-in-c/
#include <sstream>
std::vector<std::string> split(const std::string& s, char delimiter)
{
	std::vector<std::string> tokens;
	std::string token;
	std::istringstream tokenStream(s);
	while (std::getline(tokenStream, token, delimiter)) {
		tokens.push_back(token);
	}
	return tokens;
}

int main(int argc, char** argv)
{
	unsigned int serialBaud = 115200;
  std::string serialPort = "/dev/ttyS5"; // UART5 on the Bela
	std::string remote = "localhost:57120"; // OSC remote to send messages to.. should be sclang

  // PARSE COMMAND LINE ARGUMENTS
  int c = 1;
	if(argc == 1) {
		printf(helpText, argv[0], remote.c_str());
		return 0;
	}
  while(c < argc) {
    if(std::string("--help") == std::string(argv[c])) {
      printf(helpText, argv[0], remote.c_str());
      return 0;
    }
		if(std::string("--port") == std::string(argv[c])) {
      ++c;
      if(c < argc) {
				serialPort = argv[c];
        if(serialPort == "") {
          fprintf(stderr, "Invalid serial port: %s\n", argv[c]);
        }
      }
    }
		if(std::string("--baud") == std::string(argv[c])) {
      ++c;
      if(c < argc) {
				serialBaud = atoi(argv[c]);
      }
    }
		if(std::string("--remote") == std::string(argv[c])) {
			++c;
			if(c < argc) {
				remote = argv[c];
				if(remote == "") {
					fprintf(stderr, "Invalid OSC remote server: %s\n", argv[c]);
				}
			}
		}
		if(std::string("--verbose") == std::string(argv[c])) {
			++c;
			gVerbose = true;
		}
    ++c;
  }

	std::vector<std::string> spl = split(remote, ':');
	if(2 != spl.size()) {
		fprintf(stderr, "Wrong or unparseable `IP:port` argument: %s\n", remote.c_str());
		return 1;
	}
	std::string remoteHost = spl[0];
	unsigned int remotePort = atoi(spl[1].c_str());

	signal(SIGINT, interrupt_handler); // set up user interrupt handler...

	std::cout << "Connecting to serial port " << serialPort << " at " << serialBaud << " baud" << "\n";
  gSerial.setup(serialPort.c_str(), serialBaud);

	std::cout << "Sending OSC to " << remoteHost << ":" << remotePort << "\n";
	gSock.connectTo(remoteHost, remotePort);
	std::string address = "/serial";


	int ret = 0;
	unsigned int maxLen = 128;
	char serialBuffer[maxLen];
	while(ret < 1 && !gShouldStop) {
		if(gVerbose)
			printf("Waiting for serial ...\n");
		usleep(1000000);
		ret = gSerial.read(serialBuffer, maxLen, 100);
	}

  // Serial Read Loop
	while(!gShouldStop) {
		// read from the serial port with a timeout of 100ms
		if (ret > 0) {
			if(gVerbose)
				printf("READ: %.*s\n", ret, serialBuffer);

			// ignore lines that are longer than 60 bytes (ex return) "1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,1000,"
			// ignore lines that are shorter than 24 bytes (ex return) "0,0,0,0,0,0,0,0,0,0,0,0,"

			if(ret < 24 || ret > 62) {
				fprintf(stderr, "Invalid packet length: %i\n", ret);
			} else {
				// Parse the Serial Packet
				std::string packet(serialBuffer, ret);
				std::vector<std::string> tokens = split(packet, ',');

				if(tokens.size() != 13) {
					fprintf(stderr, "Invalid number of values in packet: %i\n", tokens.size());
				} else {
					// Send OSC message!
					oscpkt::Message msg(address);

					for(unsigned int i = 0; i < 12; i++) {
						if(gVerbose)
							printf("%i,", atoi(tokens[i].c_str()));
						msg.pushInt32(atoi(tokens[i].c_str()));
					}
					int success = sendOsc(msg);
					if(success == 0) {
						if(gVerbose)
							printf(" ~~~ SENT! \n");
					} else {
						if(gVerbose)
							printf(" ~~~ ERROR %i! \n", success);
					}
				}
			}
		}

		// Serial Read
		ret = gSerial.read(serialBuffer, maxLen, 100);
	}

	return 0;
}
