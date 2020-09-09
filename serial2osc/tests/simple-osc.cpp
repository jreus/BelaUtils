/****
Simple OSC message sending. Targets sclang on port 57120

compile using
g++ -g -Wall simple-osc.cpp -o sosc -I..

*****/

#include <string>
#include <memory>
#include <map>
#include <unistd.h>

#define OSCPKT_OSTREAM_OUTPUT
#include "libraries/OSC/oscpkt.hh"
#include <iostream> // needed for udp.hh
#include "libraries/OSC/udp.hh"

#include <signal.h>

int gShouldStop = 0;
int verbose = true;

void interrupt_handler(int var)
{
	gShouldStop = true;
}

oscpkt::UdpSocket gSock;

// OSC FUNCTIONS
int sendOsc(const oscpkt::Message& msg)
{
	oscpkt::PacketWriter pw;
	pw.addMessage(msg);
	bool ok = gSock.sendPacket(pw.packetData(), pw.packetSize());
	if(!ok) {
		fprintf(stderr, "Error sending message\n");
		return -1;
	} else {
		if(verbose)
			printf("Sent message ...");
	}
	return 0;
}


int main(int argc, char** argv)
{
  const std::string remoteHost = "localhost"; // OSC remote to send messages to.. should be sclang
	const unsigned int remotePort = 57120;

	signal(SIGINT, interrupt_handler); // stop the while loop gracefully on interrupt

	std::cout << "Connecting to " << remoteHost << ":" << remotePort << "\n";
	gSock.connectTo(remoteHost, remotePort);
	std::string address = "/serial";

	// Send OSC messages

	oscpkt::Message msg(address);
	msg.pushFloat(678);
	msg.pushStr("This is a string");
	msg.pushInt32(1020);

	while(!gShouldStop) {
		sendOsc(msg);
		usleep(1000000); // sleep for 1s
	}
	return 0;
}
