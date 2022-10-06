#pragma once
#include"CustomPrint.h"

enum class Headers {

};

struct SectionHeader {
	Headers header;
	size_t messageLength;
	size_t messageID;
};

// Header for sending information about next message
struct  MessageHeader {
	char message[1024];
};
