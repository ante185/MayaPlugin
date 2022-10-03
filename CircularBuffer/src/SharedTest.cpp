#include"Comlib.h"

void RandString(char* s, const size_t& size) {
    static const char alphanum[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";

    for (size_t i = 0u; i < size; ++i)
        s[i] = alphanum[std::rand() % (int)(sizeof(alphanum) - 1ull)];

    s[size] = '\0';
}

int main(int argC, char* argV[]) {
    SET_DEBUG_FLAGS;

    if(argC > 1) {
        DWORD delay = atoi(argV[2]);
        size_t memorySize((size_t)atoi(argV[3]) * (1ull << 10));
        size_t messages = atoi(argV[4]);
        size_t messageSize = 0ull;
        char* message = NEW char[memorySize];
        bool done(false);
        MessageHeader* messageHeader = NEW MessageHeader();

        ProcessType type = (!strcmp(argV[1], "Producer")) ? ProcessType::Producer : ProcessType::Consumer;
        Comlib com(L"Buffer", memorySize, type);

        if(type == ProcessType::Producer) {

            for(size_t i = 0ull; i < messages; i++) {
                if(!strcmp(argV[5], "random")) {
                    messageSize = (size_t)(std::rand() % (int)memorySize / 4) + 1;
                    RandString(message, messageSize);
                } else {
                    messageSize = (size_t)(atoi(argV[5]));
                    RandString(message, messageSize);
                }

                messageHeader->messageLength = messageSize;

                while(!done) {
                    Sleep(delay);
                    done = com.Send(message, messageHeader);
                    if(done) Print("Message: {0} Sent: {1}\n", i + 1, (char*)message);
                }

                done = false;
            } 

        } else if(type == ProcessType::Consumer) {

            for(size_t i = 0ull; i < messages; i++) {
            
                while(!done) {
                    Sleep(delay);
                    memset(message, '\0', memorySize);
                    done = com.Recieve(message);
                    if(done) Print("Message: {0} Recieved: {1}\n", i + 1, (char*)message);
                }

                done = false;
            }

        }

        delete[] message;
        delete messageHeader;
    }

    Print('\n');
    system("PAUSE");
    return 0;
}