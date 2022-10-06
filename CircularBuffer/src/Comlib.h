#pragma once
#include <string>
#include "Memory.h"
#include "Headers.h"
#include "Mutex.h"

enum class ProcessType {
	Producer, 
	Consumer
};

class Comlib {
	public:
	Comlib(LPCWSTR bufferName, size_t bufferSize, ProcessType type);
	~Comlib();

	Memory* GetSharedMemory() {return mp_sharedMemory;}
	bool Send(char* message, SectionHeader* secHeader);
	bool Recieve(char* message, SectionHeader*& mp_messageHeader);

	private:
	Mutex* mp_mutex;
	Memory* mp_sharedMemory;
	char* mp_messageData;

	size_t* mp_head;
	size_t* mp_tail;
	size_t* mp_freeMemory;


	ControlHeader* mp_ctrler;
	const ProcessType m_type;
};