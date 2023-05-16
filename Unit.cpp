#include "Unit.h"

uint64_t Unit::instance_count = 0;

uint64_t Unit::getTime() {
    FILETIME filetime;
    uint64_t result;
    do
    {
        GetSystemTimePreciseAsFileTime(&filetime);
        result = ULARGE_INTEGER { filetime.dwLowDateTime, filetime.dwHighDateTime }.QuadPart;
    } while (result == last_getTime);
    last_getTime = result;
    return result;
}

void Unit::handleConfirmation(uint64_t timestamp, Unit::PeerDataSend &peer) {
    std::lock_guard lock(peer.mutex_send);
    if (peer.timestamp == timestamp)
        peer.sent = true;
}

void Unit::handlePayload(const uint8_t *data, uint16_t length, Unit::PeerDataReceive &peer, const SOCKADDR_IN &from) {
    if (length)
        for (uint16_t i = 0; i < length; ++i)
            peer.data.push_back(data[i]);
    else
    {
        receiveData(peer.data.data(), peer.data.size(), ntohl(from.sin_addr.S_un.S_addr));
        peer.data.clear();
    }
}

void Unit::sendConfirmation(uint64_t timestamp, const SOCKADDR_IN &to) const {
    char packet[sizeof(uint8_t) + sizeof(uint64_t)];
    packet[0] = 1;
    std::memcpy(packet + sizeof(uint8_t), &timestamp, sizeof(uint64_t));
    sendto(handle, packet, sizeof(packet), 0,
           reinterpret_cast<const SOCKADDR*>(&to), sizeof(to));
}

void Unit::handlePacket(uint8_t is_confirmation, uint64_t timestamp, const uint8_t *data, uint16_t length,
                        const SOCKADDR_IN &from) {
    std::shared_ptr<PeerDataReceive> peer_receive;
    std::shared_ptr<PeerDataSend> peer_send;
    {
        uint32_t address = ntohl(from.sin_addr.S_un.S_addr);
        std::lock_guard lock(mutex_peers);
        if (!peers.contains(address))
            peers[address] = std::make_tuple(std::make_shared<PeerDataReceive>(), std::make_shared<PeerDataSend>());
        peer_receive = std::get<0>(peers[address]);
        peer_send = std::get<1>(peers[address]);
    }
    if (is_confirmation)
        handleConfirmation(timestamp, *peer_send);
    else
    {
        if (peer_receive->timestamp < timestamp)
        {
            peer_receive->timestamp = timestamp;
            handlePayload(data, length, *peer_receive, from);
        }
        sendConfirmation(timestamp, from);
    }
}

void Unit::receiveLoop() {
    while (!stop.load())
    {
        SOCKADDR_IN from;
        ZeroMemory(&from, sizeof(from));
        int from_length = sizeof(from);
        char packet[1 << 16];
        int byte_count = recvfrom(handle, packet, sizeof(packet), 0, reinterpret_cast<SOCKADDR*>(&from), &from_length);
        if ((byte_count >= (sizeof(uint8_t) + sizeof(uint64_t))) && (from.sin_family = AF_INET))
        {
            uint8_t is_confirmation;
            std::memcpy(&is_confirmation, packet, sizeof(uint8_t));
            uint64_t timestamp;
            std::memcpy(&timestamp, packet + sizeof(uint8_t), sizeof(uint64_t));
            handlePacket(is_confirmation, timestamp,
                         reinterpret_cast<uint8_t*>(packet + sizeof(uint8_t) + sizeof(uint64_t)),
                         byte_count - sizeof(uint8_t) - sizeof(uint64_t),
                         from);
        }
    }
}

Unit::Unit(uint16_t port, uint32_t timeout) : last_getTime(0), port(port), peers(), mutex_peers()
{
    if ((++instance_count) == 1)
    {
        WSADATA wsa_data;
        ZeroMemory(&wsa_data, sizeof(wsa_data));
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data))
        {
            instance_count = 0;
            throw UnitException("WSAStartup");
        }
    }
    if ((handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET)
    {
        if ((--instance_count) == 0)
            WSACleanup();
        throw UnitException("socket");
    }
    {
        SOCKADDR_IN name;
        ZeroMemory(&name, sizeof(name));
        name.sin_family = AF_INET;
        name.sin_port = htons(port);
        name.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
        if (bind(handle, reinterpret_cast<SOCKADDR*>(&name), sizeof(name)))
        {
            closesocket(handle);
            if ((--instance_count) == 0)
                WSACleanup();
            throw UnitException("bind");
        }
    }
    if (setsockopt(handle, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(DWORD)))
    {
        closesocket(handle);
        if ((--instance_count) == 0)
            WSACleanup();
        throw UnitException("setsockopt");
    }
    if (setsockopt(handle, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<char*>(&timeout), sizeof(DWORD)))
    {
        closesocket(handle);
        if ((--instance_count) == 0)
            WSACleanup();
        throw UnitException("setsockopt");
    }
    stop.store(false);
    thread_loop = std::thread(&Unit::receiveLoop, this);
}

Unit::~Unit() {
    stop.store(true);
    thread_loop.join();
    closesocket(handle);
    if ((--instance_count) == 0)
        WSACleanup();
}

void Unit::sendData(const uint8_t *data, uint64_t length, uint32_t address) {
    SOCKADDR_IN to;
    ZeroMemory(&to, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(port);
    to.sin_addr.S_un.S_addr = htonl(address);
    std::shared_ptr<PeerDataSend> peer;
    {
        std::lock_guard lock(mutex_peers);
        if (!peers.contains(address))
            peers[address] = std::make_tuple(std::make_shared<PeerDataReceive>(), std::make_shared<PeerDataSend>());
        peer = std::get<1>(peers[address]);
    }
    uint64_t bytes_sent = 0;
    while (bytes_sent != length)
    {
        {
            std::lock_guard lock(peer->mutex_send);
            peer->timestamp = getTime();
            peer->sent = false;
        }
        uint64_t payload_size = length - bytes_sent;
        if (payload_size > (packet_size - sizeof(uint8_t) - sizeof(uint64_t)))
            payload_size = packet_size - sizeof(uint8_t) - sizeof(uint64_t);
        char packet[packet_size];
        packet[0] = 0;
        std::memcpy(packet + sizeof(uint8_t), &peer->timestamp, sizeof(uint64_t));
        std::memcpy(packet + sizeof(uint8_t) + sizeof(uint64_t), data + bytes_sent, payload_size);
        while (true)
        {
            {
                std::lock_guard lock(peer->mutex_send);
                if (peer->sent)
                    break;
            }
            sendto(handle, packet, static_cast<int>(sizeof(uint8_t) + sizeof(uint64_t) + payload_size), 0,
                   reinterpret_cast<SOCKADDR*>(&to), sizeof(to));
        }
        bytes_sent += payload_size;
    }
    {
        std::lock_guard lock(peer->mutex_send);
        peer->timestamp = getTime();
        peer->sent = false;
    }
    char packet[sizeof(uint8_t) + sizeof(uint64_t)];
    packet[0] = 0;
    std::memcpy(packet + sizeof(uint8_t), &peer->timestamp, sizeof(uint64_t));
    while (true)
    {
        {
            std::lock_guard lock(peer->mutex_send);
            if (peer->sent)
                break;
        }
        sendto(handle, packet, sizeof(packet), 0,
               reinterpret_cast<SOCKADDR*>(&to), sizeof(to));
    }
}

Unit::PeerDataReceive::PeerDataReceive() : timestamp(0), data()
{
}

Unit::PeerDataSend::PeerDataSend() : mutex_send(), sent(false), timestamp(0)
{
}

UnitException::UnitException(const char *message) : message(message)
{
}

const char *UnitException::what() const noexcept { return message.c_str(); }
