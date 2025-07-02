#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <sstream>
#include <atomic>
#include <errno.h>

// 서버와 동일한 패킷 헤더 구조
struct PacketHeader
{
    std::uint16_t size;
    std::uint16_t id;
};

class TestClient
{
public:
    TestClient(const std::string &host, int port) : host_(host), port_(port), sock_(-1) {}

    ~TestClient()
    {
        disconnect();
    }

    bool connect()
    {
        sock_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_ < 0)
        {
            std::cerr << "소켓 생성 실패" << std::endl;
            return false;
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port_);

        if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0)
        {
            std::cerr << "잘못된 주소: " << host_ << std::endl;
            close(sock_);
            sock_ = -1;
            return false;
        }

        if (::connect(sock_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            std::cerr << "서버 연결 실패: " << strerror(errno) << std::endl;
            close(sock_);
            sock_ = -1;
            return false;
        }

        std::cout << "서버에 연결되었습니다: " << host_ << ":" << port_ << std::endl;
        return true;
    }

    void disconnect()
    {
        if (sock_ >= 0)
        {
            close(sock_);
            sock_ = -1;
            std::cout << "서버와의 연결을 종료했습니다." << std::endl;
        }
    }

    bool sendPacket(std::uint16_t packetId, const std::vector<std::uint8_t> &data = {})
    {
        if (sock_ < 0)
        {
            std::cerr << "연결되지 않음" << std::endl;
            return false;
        }

        // 패킷 생성
        std::vector<std::uint8_t> packet;
        PacketHeader header;
        header.size = sizeof(PacketHeader) + data.size();
        header.id = packetId;

        // 헤더 추가
        packet.resize(sizeof(PacketHeader));
        std::memcpy(packet.data(), &header, sizeof(PacketHeader));

        // 데이터 추가
        if (!data.empty())
        {
            packet.insert(packet.end(), data.begin(), data.end());
        }

        // 패킷 전송
        ssize_t sent = send(sock_, packet.data(), packet.size(), 0);
        if (sent < 0)
        {
            std::cerr << "패킷 전송 실패: " << strerror(errno) << std::endl;
            return false;
        }

        std::cout << "✓ 패킷 전송 완료 - ID: " << packetId
                  << ", 크기: " << packet.size()
                  << ", 전송된 바이트: " << sent << std::endl;
        return true;
    }

    bool sendStringMessage(std::uint16_t packetId, const std::string &message)
    {
        std::vector<std::uint8_t> data(message.begin(), message.end());
        return sendPacket(packetId, data);
    }

    bool receivePacket()
    {
        if (sock_ < 0)
        {
            std::cerr << "연결되지 않음" << std::endl;
            return false;
        }

        // 헤더 수신
        PacketHeader header;
        ssize_t received = recv(sock_, &header, sizeof(PacketHeader), MSG_WAITALL);
        if (received <= 0)
        {
            if (received == 0)
            {
                std::cout << "서버가 연결을 종료했습니다." << std::endl;
            }
            else
            {
                std::cerr << "헤더 수신 실패: " << strerror(errno) << std::endl;
            }
            return false;
        }

        std::cout << "📥 패킷 수신 - ID: " << header.id << ", 크기: " << header.size << std::endl;

        // 추가 데이터가 있다면 수신
        if (header.size > sizeof(PacketHeader))
        {
            std::size_t dataSize = header.size - sizeof(PacketHeader);
            std::vector<std::uint8_t> data(dataSize);

            received = recv(sock_, data.data(), dataSize, MSG_WAITALL);
            if (received != static_cast<ssize_t>(dataSize))
            {
                std::cerr << "데이터 수신 실패" << std::endl;
                return false;
            }

            // 문자열로 출력 시도
            std::string dataStr(data.begin(), data.end());
            std::cout << "📄 수신된 메시지: \"" << dataStr << "\"" << std::endl;
        }

        return true;
    }

    void startReceiveThread()
    {
        receive_thread_running_ = true;
        receive_thread_ = std::thread([this]()
                                      {
            std::cout << "🔄 수신 스레드 시작됨" << std::endl;
            while (receive_thread_running_)
            {
                std::cout << "⏳ 서버 응답 대기 중..." << std::endl;
                if (!receivePacket())
                {
                    std::cout << "❌ 패킷 수신 실패 또는 연결 종료" << std::endl;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            std::cout << "🏁 수신 스레드 종료됨" << std::endl; });
    }

    void stopReceiveThread()
    {
        receive_thread_running_ = false;
        if (receive_thread_.joinable())
        {
            receive_thread_.join();
        }
    }

private:
    std::string host_;
    int port_;
    int sock_;
    std::thread receive_thread_;
    std::atomic<bool> receive_thread_running_{false};
};

void showMenu()
{
    std::cout << "\n========== 메뉴 ==========" << std::endl;
    std::cout << "1. Welcome 패킷 전송 (ID: 1)" << std::endl;
    std::cout << "2. Player Move 패킷 전송 (ID: 2)" << std::endl;
    std::cout << "3. Chat 메시지 전송 (ID: 3)" << std::endl;
    std::cout << "4. 사용자 정의 메시지 전송" << std::endl;
    std::cout << "5. 서버 연결 상태 확인" << std::endl;
    std::cout << "6. 자동 테스트 (연속 메시지 전송)" << std::endl;
    std::cout << "0. 종료" << std::endl;
    std::cout << "=========================" << std::endl;
    std::cout << "선택하세요 (0-6): ";
}

int main()
{
    std::cout << "🎮 === 게임서버 대화형 클라이언트 === 🎮" << std::endl;
    std::cout << "서버 주소와 포트를 입력하세요" << std::endl;

    std::string host;
    int port;

    std::cout << "호스트 (기본값: 127.0.0.1): ";
    std::getline(std::cin, host);
    if (host.empty())
    {
        host = "127.0.0.1";
    }

    std::cout << "포트 (기본값: 8080): ";
    std::string portStr;
    std::getline(std::cin, portStr);
    if (portStr.empty())
    {
        port = 8080;
    }
    else
    {
        port = std::stoi(portStr);
    }

    TestClient client(host, port);

    if (!client.connect())
    {
        std::cout << "❌ 서버 연결에 실패했습니다." << std::endl;
        return 1;
    }

    // 서버 응답을 자동으로 수신하는 스레드 시작
    client.startReceiveThread();

    int choice;
    std::string input;

    while (true)
    {
        showMenu();
        std::getline(std::cin, input);

        try
        {
            choice = std::stoi(input);
        }
        catch (...)
        {
            std::cout << "❌ 잘못된 입력입니다. 숫자를 입력해주세요." << std::endl;
            continue;
        }

        switch (choice)
        {
        case 1:
            std::cout << "📤 Welcome 패킷을 전송합니다..." << std::endl;
            if (client.sendPacket(1))
            {
                std::cout << "✅ Welcome 패킷 전송 성공" << std::endl;
            }
            else
            {
                std::cout << "❌ Welcome 패킷 전송 실패" << std::endl;
            }
            break;

        case 2:
        {
            std::cout << "📤 Player Move 패킷을 전송합니다..." << std::endl;
            std::vector<std::uint8_t> moveData = {10, 20, 30, 40}; // 임시 이동 데이터
            if (client.sendPacket(2, moveData))
            {
                std::cout << "✅ Player Move 패킷 전송 성공" << std::endl;
            }
            else
            {
                std::cout << "❌ Player Move 패킷 전송 실패" << std::endl;
            }
        }
        break;

        case 3:
        {
            std::cout << "💬 전송할 채팅 메시지를 입력하세요: ";
            std::string chatMessage;
            std::getline(std::cin, chatMessage);

            if (!chatMessage.empty())
            {
                client.sendStringMessage(3, chatMessage);
            }
            else
            {
                std::cout << "❌ 빈 메시지는 전송할 수 없습니다." << std::endl;
            }
        }
        break;

        case 4:
        {
            std::cout << "패킷 ID를 입력하세요 (1-65535): ";
            std::string packetIdStr;
            std::getline(std::cin, packetIdStr);

            try
            {
                int packetId = std::stoi(packetIdStr);
                if (packetId < 1 || packetId > 65535)
                {
                    std::cout << "❌ 패킷 ID는 1-65535 범위여야 합니다." << std::endl;
                    break;
                }

                std::cout << "전송할 메시지를 입력하세요: ";
                std::string message;
                std::getline(std::cin, message);

                client.sendStringMessage(static_cast<std::uint16_t>(packetId), message);
            }
            catch (...)
            {
                std::cout << "❌ 잘못된 패킷 ID입니다." << std::endl;
            }
        }
        break;

        case 5:
            std::cout << "🔍 연결 상태: 활성" << std::endl;
            std::cout << "서버 주소: " << host << ":" << port << std::endl;
            break;

        case 6:
        {
            std::cout << "🔄 자동 테스트 시작 - 연속으로 5개 메시지 전송" << std::endl;
            for (int i = 1; i <= 5; ++i)
            {
                std::cout << "\n--- 테스트 메시지 " << i << "/5 ---" << std::endl;

                if (client.sendPacket(1)) // Welcome 패킷
                {
                    std::cout << "✅ 메시지 " << i << " 전송 성공" << std::endl;
                    // 서버 응답을 기다림
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
                else
                {
                    std::cout << "❌ 메시지 " << i << " 전송 실패" << std::endl;
                    break;
                }
            }
            std::cout << "🏁 자동 테스트 완료" << std::endl;
        }
        break;

        case 0:
            std::cout << "👋 클라이언트를 종료합니다..." << std::endl;
            client.stopReceiveThread();
            return 0;

        default:
            std::cout << "❌ 잘못된 선택입니다. 0-6 사이의 숫자를 입력해주세요." << std::endl;
            break;
        }

        // 잠시 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    return 0;
}