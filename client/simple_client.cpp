#include <iostream>
#include <string>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <thread>
#include <chrono>

class SimpleClient
{
public:
    SimpleClient(const std::string &host, int port)
        : host_(host), port_(port), sock_(-1) {}

    ~SimpleClient()
    {
        disconnect();
    }

    bool connect()
    {
        sock_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sock_ < 0)
        {
            std::cerr << "❌ 소켓 생성 실패" << std::endl;
            return false;
        }

        sockaddr_in server_addr{};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(port_);

        if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0)
        {
            std::cerr << "❌ 주소 변환 실패: " << host_ << std::endl;
            return false;
        }

        if (::connect(sock_, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        {
            std::cerr << "❌ 서버 연결 실패: " << host_ << ":" << port_ << std::endl;
            return false;
        }

        std::cout << "✅ 서버 연결 성공: " << host_ << ":" << port_ << std::endl;
        return true;
    }

    void disconnect()
    {
        if (sock_ >= 0)
        {
            close(sock_);
            sock_ = -1;
            std::cout << "🔌 서버 연결 해제" << std::endl;
        }
    }

    bool sendMessage(const std::string &message)
    {
        if (sock_ < 0)
        {
            std::cerr << "❌ 연결되지 않음" << std::endl;
            return false;
        }

        ssize_t sent = send(sock_, message.c_str(), message.length(), 0);
        if (sent < 0)
        {
            std::cerr << "❌ 메시지 전송 실패" << std::endl;
            return false;
        }

        std::cout << "📤 전송: " << message << " (" << sent << " bytes)" << std::endl;
        return true;
    }

    std::string receiveMessage()
    {
        if (sock_ < 0)
        {
            return "";
        }

        char buffer[1024] = {0};
        ssize_t received = recv(sock_, buffer, sizeof(buffer) - 1, 0);

        if (received < 0)
        {
            std::cerr << "❌ 메시지 수신 실패" << std::endl;
            return "";
        }

        if (received == 0)
        {
            std::cout << "🔌 서버가 연결을 닫았습니다" << std::endl;
            return "";
        }

        std::string message(buffer, received);
        std::cout << "📥 수신: " << message << " (" << received << " bytes)" << std::endl;
        return message;
    }

private:
    std::string host_;
    int port_;
    int sock_;
};

int main()
{
    std::cout << "🎮 게임 서버 테스트 클라이언트 시작" << std::endl;

    SimpleClient client("127.0.0.1", 8080);

    if (!client.connect())
    {
        return 1;
    }

    // 테스트 메시지들
    std::vector<std::string> test_messages = {
        "Hello Server!",
        "Test Message 1",
        "게임 연결 테스트",
        "Echo Test 123",
        "Final Message"};

    for (const auto &msg : test_messages)
    {
        if (!client.sendMessage(msg))
        {
            break;
        }

        // 에코 응답 대기
        std::string response = client.receiveMessage();
        if (response.empty())
        {
            break;
        }

        // 응답 확인
        if (response == msg)
        {
            std::cout << "✅ 에코 성공: " << msg << std::endl;
        }
        else
        {
            std::cout << "⚠️ 에코 불일치 - 전송: '" << msg << "', 수신: '" << response << "'" << std::endl;
        }

        // 잠시 대기
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "🏁 테스트 완료" << std::endl;
    return 0;
}