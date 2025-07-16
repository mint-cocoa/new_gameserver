#include "include/session_manager.h"
#include "../io/include/buffer_ring.h"
#include "../coroutine/include/spawn.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>

namespace co_uring
{

    // GameSession 구현
    GameSession::GameSession(std::unique_ptr<socket_client> client, std::string session_id)
        : client_(std::move(client)), session_id_(std::move(session_id)),
          last_heartbeat_(std::chrono::steady_clock::now())
    {
        LOG_INFO("🎮 GameSession 생성: {}", session_id_);
    }

    GameSession::~GameSession()
    {
        LOG_INFO("🎮 GameSession 소멸: {}", session_id_);
    }

    void GameSession::UpdateHeartbeat() noexcept
    {
        last_heartbeat_ = std::chrono::steady_clock::now();
        player_data_.last_activity = last_heartbeat_;
    }

    bool GameSession::IsExpired(std::chrono::minutes timeout) const noexcept
    {
        return std::chrono::steady_clock::now() - last_heartbeat_ > timeout;
    }

    void GameSession::OnConnected()
    {
        LOG_INFO("🔗 플레이어 연결됨: 세션 ID {}", session_id_);
        connected_.store(true);
        UpdateHeartbeat();
    }

    void GameSession::OnDisconnected()
    {
        LOG_INFO("🔌 플레이어 연결 해제됨: 세션 ID {}", session_id_);
        connected_.store(false);
    }

    void GameSession::OnRecvData(const std::uint8_t *buffer, std::size_t len)
    {
        LOG_DEBUG("📥 데이터 수신: 세션 {} - {} bytes", session_id_, len);
        UpdateHeartbeat();

        // 기본적으로 에코 구현 (테스트용)
        std::vector<std::uint8_t> echo_data(buffer, buffer + len);
        spawn(SendData(std::move(echo_data)));
    }

    task<void> GameSession::SendData(std::vector<std::uint8_t> &&buffer)
    {
        if (!client_ || !connected_.load())
        {
            LOG_WARN("⚠️ 연결되지 않은 세션에 데이터 전송 시도: {}", session_id_);
            co_return;
        }

        try
        {
            std::span<const std::uint8_t> data_span(buffer.data(), buffer.size());
            auto result = co_await client_->send(data_span);

                    if (result >= 0)
        {
            LOG_DEBUG("📤 데이터 전송 완료: 세션 {} - {} bytes", session_id_, result);
        }
        else
        {
            LOG_ERROR("❌데이터 전송 실패: 세션 {} - error code {}", session_id_, result);
        }
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("❌ 데이터 전송 예외: 세션 {} - {}", session_id_, e.what());
        }
    }

    // SessionManager 구현
    SessionManager &SessionManager::GetInstance() noexcept
    {
        static SessionManager instance;
        return instance;
    }

    std::string SessionManager::AddSession(std::unique_ptr<socket_client> client)
    {
        std::unique_lock lock{mutex_};

        std::string session_id = GenerateSessionId();
        auto session = std::make_shared<GameSession>(std::move(client), session_id);
        sessions_[session_id] = session;

        LOG_INFO("✨ 새 세션 생성: {} (총 세션 수: {})", session_id, sessions_.size());
        return session_id;
    }

    std::shared_ptr<GameSession> SessionManager::GetSession(const std::string &session_id)
    {
        std::shared_lock lock{mutex_};

        if (auto it = sessions_.find(session_id); it != sessions_.end())
        {
            return it->second;
        }
        return nullptr;
    }

    void SessionManager::RemoveSession(const std::string &session_id)
    {
        std::unique_lock lock{mutex_};

        if (auto it = sessions_.find(session_id); it != sessions_.end())
        {
            LOG_INFO("🗑️ 세션 제거: {} (남은 세션 수: {})", session_id, sessions_.size() - 1);
            sessions_.erase(it);
        }
    }

    void SessionManager::CleanupExpiredSessions()
    {
        std::unique_lock lock{mutex_};

        auto now = std::chrono::steady_clock::now();
        size_t removed_count = 0;

        for (auto it = sessions_.begin(); it != sessions_.end();)
        {
            if (it->second->IsExpired())
            {
                LOG_INFO("⏰ 만료된 세션 정리: {}", it->first);
                it = sessions_.erase(it);
                ++removed_count;
            }
            else
            {
                ++it;
            }
        }

        if (removed_count > 0)
        {
            LOG_INFO("🧹 세션 정리 완료: {} 개 세션 제거", removed_count);
        }
    }

    std::size_t SessionManager::GetActiveSessionCount() const noexcept
    {
        std::shared_lock lock{mutex_};
        return sessions_.size();
    }

    task<void> SessionManager::HandleSession(std::shared_ptr<GameSession> session)
    {
        if (!session)
        {
            LOG_ERROR("❌ null 세션을 처리하려고 시도");
            co_return;
        }

        auto session_id = session->GetSessionId();
        LOG_INFO("🔄 세션 처리 시작: {}", session_id);

        try
        {
            session->OnConnected();

            auto &socket = session->GetSocket();

            while (session && socket.is_valid())
            {
                auto recv_result = co_await socket.recv();

                if (recv_result < 0)
                {
                    LOG_WARN("⚠️ 수신 오류: 세션 {} - error code {}", session_id, recv_result);
                    break;
                }

                if (recv_result == 0)
                {
                    LOG_INFO("🔌 클라이언트 연결 종료: 세션 {}", session_id);
                    break;
                }

                // 버퍼 링에서 데이터 가져오기
                auto &buffer_ring = BufferRing::getInstance();
                auto recv_awaiter = socket.recv();
                auto buffer_id = recv_awaiter.get_buffer_id();
                auto buffer_size = recv_awaiter.get_buffer_size();

                auto& buffer_data = buffer_ring.borrowBuf(buffer_id);
                            if (!buffer_data.empty() && buffer_size > 0)
            {
                session->OnRecvData(buffer_data.data(), buffer_size);
                buffer_ring.returnBuf(buffer_id);
            }
            }
        }
        catch (const std::exception &e)
        {
            LOG_ERROR("❌ 세션 처리 중 예외: {} - {}", session_id, e.what());
        }

        session->OnDisconnected();
        RemoveSession(session_id);

        LOG_INFO("🔚 세션 처리 종료: {}", session_id);
    }

    std::string SessionManager::GenerateSessionId()
    {
        static std::random_device rd;
        static std::mt19937 gen{rd()};
        static std::uniform_int_distribution<> dis{0, 15};
        static const char *chars = "0123456789ABCDEF";

        std::string id;
        id.reserve(32);
        for (int i = 0; i < 32; ++i)
        {
            id += chars[dis(gen)];
        }
        return id;
    }

    // 전역 함수 구현
    task<void> HandleClientSession(std::unique_ptr<socket_client> client)
    {
        if (!client)
        {
            LOG_ERROR("❌ null 클라이언트로 세션 처리 시도");
            co_return;
        }

        auto &session_manager = SessionManager::GetInstance();
        auto session_id = session_manager.AddSession(std::move(client));
        auto session = session_manager.GetSession(session_id);

        if (session)
        {
            co_await session_manager.HandleSession(session);
        }
        else
        {
            LOG_ERROR("❌ 세션 생성 후 즉시 찾을 수 없음: {}", session_id);
        }
    }

} // namespace co_uring