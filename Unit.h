#ifndef NETWORKING_UNIT_H
#define NETWORKING_UNIT_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>
#include <winsock2.h>

#include <cstdint>
#include <cstring>
#include <exception>
#include <string>
#include <thread>
#include <map>
#include <memory>
#include <vector>
#include <tuple>
#include <atomic>
#include <mutex>
#include <chrono>

class UnitException : public std::exception
{
private:
    const std::string message;

public:
    explicit UnitException(const char* message);

    [[nodiscard]] const char* what() const noexcept override;
};

class Unit
{
private:
    struct PeerDataReceive
    {
        uint64_t timestamp;
        std::vector<uint8_t> data;

        PeerDataReceive();
    };

    struct PeerDataSend
    {
        std::mutex mutex_send;

        bool sent;
        uint64_t timestamp;

        PeerDataSend();
    };

private:
    static constexpr uint16_t packet_size = 508;

    static uint64_t instance_count;

private:
    uint64_t last_getTime;

    const uint16_t port;

    SOCKET handle;

    std::atomic_bool stop;

    std::thread thread_loop;

    std::map<uint32_t, std::tuple<std::shared_ptr<PeerDataReceive>, std::shared_ptr<PeerDataSend>>> peers;

    std::mutex mutex_peers;

private:
    uint64_t getTime();

private:
    static void handleConfirmation(uint64_t timestamp, PeerDataSend& peer);

    void handlePayload(const uint8_t* data, uint16_t length, PeerDataReceive& peer, const SOCKADDR_IN& from);

    void sendConfirmation(uint64_t timestamp, const SOCKADDR_IN& to) const;

    void handlePacket(uint8_t is_confirmation, uint64_t timestamp, const uint8_t* data, uint16_t length, const SOCKADDR_IN& from);

    void receiveLoop();

protected:
    virtual void receiveData(const uint8_t* data, uint64_t length, uint32_t address) = 0;

public:
    Unit(uint16_t port, uint32_t timeout);

    ~Unit();

    void sendData(const uint8_t* data, uint64_t length, uint32_t address);
};

#endif 
