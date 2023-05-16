#define MSG_LEN static_cast<int>(1 << 16)

#include <iostream>

#include "Unit.h"

class MyUnit : public Unit
{
private:
    void receiveData(const uint8_t* data, uint64_t length, uint32_t address) override
    {
        bool ok = true;
        for (uint64_t i = 0; i < (length - 1); ++i)
        {
            if (data[i] != ('a' + (i % 26)))
            {
                ok = false;
                break;
            }
        }
        if (ok)
        {
            std::cout << "Successfully sent ";   
        }
        else
        {
            std::cout << "Failed to send ";
        }
        std::cout << length << " bytes.\n";
    }

public:
    MyUnit(uint16_t port, uint32_t timeout) : Unit(port, timeout)
    {
    }
};

const char* dest_addr = "127.0.0.1";
const uint16_t port = 7777;
const uint32_t timeout = 500;

char message[MSG_LEN];

int main()
{
    for (int i = 0; i < (MSG_LEN - 1); ++i)
        message[i] = 'a' + (i % 26);
    message[MSG_LEN - 1] = '\0';
    MyUnit u(port, timeout);
    u.sendData(reinterpret_cast<const uint8_t*>(message), strlen(message) + 1, ntohl(inet_addr(dest_addr)));
    return 0;
}
