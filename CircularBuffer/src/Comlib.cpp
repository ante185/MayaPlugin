#include "Comlib.h"

Comlib::Comlib(LPCWSTR bufferName, size_t bufferSize, ProcessType type)
    :mp_mutex(nullptr), mp_sharedMemory(nullptr), mp_messageData(nullptr), mp_head(nullptr), mp_tail(nullptr), mp_freeMemory(nullptr),
    mp_ctrler(nullptr), m_type(type) {

    mp_sharedMemory = NEW Memory(bufferName, bufferSize);
    mp_messageData = mp_sharedMemory->GetMemoryBuffer();
    mp_mutex = NEW Mutex(L"MutexMap");


    mp_head = mp_sharedMemory->GetControlBuffer();
    mp_tail = mp_head + 1;
    mp_freeMemory = mp_tail + 1;

    if(type == ProcessType::Producer) {
        Print("Producer Initialized.\n");

        *mp_head = 0ull;
        *mp_tail = 0ull;
        *mp_freeMemory = bufferSize * (1<<10);

    } else if(type == ProcessType::Consumer) {
        Print("Consumer Initialized.\n");

        *mp_tail = 0ull;

    }
}

Comlib::~Comlib() {
    delete mp_sharedMemory;
    delete mp_mutex;
}

bool Comlib::Send(char* message, SectionHeader* msgHeader) {

    mp_mutex->Lock();

    size_t memoryLeft = mp_sharedMemory->GetBufferSize() - *mp_head;
    if(msgHeader->messageLength + sizeof(SectionHeader) >= memoryLeft) {

        if(*mp_tail != 0ull) {
            msgHeader->messageID = 0ull;
            memcpy(mp_messageData + *mp_head, msgHeader, sizeof(SectionHeader));

            *mp_freeMemory -= (msgHeader->messageLength + sizeof(SectionHeader));
            *mp_head = 0ull;

        }

    } else if(msgHeader->messageLength + sizeof(SectionHeader) < *mp_freeMemory - 1ull) {
        msgHeader->messageID = 1ull;

        memcpy(mp_messageData + *mp_head, msgHeader, sizeof(SectionHeader));
        memcpy(mp_messageData + *mp_head + sizeof(SectionHeader), message, msgHeader->messageLength);

        *mp_freeMemory -= (msgHeader->messageLength + sizeof(SectionHeader));
        *mp_head = (*mp_head + msgHeader->messageLength + sizeof(SectionHeader)) % mp_sharedMemory->GetBufferSize();

        mp_mutex->Unlock();
        return true;

    }

    mp_mutex->Unlock();
    return false;
}

bool Comlib::Recieve(char* message, SectionHeader*& mp_secHeader) {

    mp_mutex->Lock();

    size_t msgLength(0ull);
    if(*mp_freeMemory < mp_sharedMemory->GetBufferSize()) {

        if(*mp_head != *mp_tail) {
            mp_secHeader = (SectionHeader*)&mp_messageData[*mp_tail];

            msgLength = mp_secHeader->messageLength;
            if(mp_secHeader->messageID == 0ull) {
                *mp_freeMemory += (msgLength + sizeof(SectionHeader));
                *mp_tail = 0ull;

            } else {
                memcpy(message, &mp_messageData[*mp_tail + sizeof(SectionHeader)], msgLength);

                *mp_freeMemory += (msgLength + sizeof(SectionHeader));
                *mp_tail = (*mp_tail + msgLength + sizeof(SectionHeader)) % mp_sharedMemory->GetBufferSize();

                mp_mutex->Unlock();
                return true;
                
            }
        }
    }

    mp_mutex->Unlock();
    return false;
}