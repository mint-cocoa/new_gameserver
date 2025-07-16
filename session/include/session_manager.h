#pragma once

#include "../../io/include/socket.h"
#include "../../io/include/logger.h"
#include "../../coroutine/include/task.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <chrono>
#include <random>
#include <cstdint>

namespace co_uring
{

    // 게임 플레이어 정보
    struct PlayerData
    {
        std::string player_id;
        std::string name;
        std::uint32_t level = 1;
        std::uint32_t experience = 0;
        std::chrono::steady_clock::time_point last_activity;

        PlayerData() : last_activity(std::chrono::steady_clock::now()) {}
    };

    // 게임 세션 클래스
    class GameSession
    {
    public:
        GameSession(std::unique_ptr<socket_client> client, std::string session_id);
        virtual ~GameSession();

        // 접근자 메서드
        socket_client &GetSocket() noexcept { return *client_; }
        const std::string &GetSessionId() const noexcept { return session_id_; }
        PlayerData &GetPlayerData() noexcept { return player_data_; }

        // 하트비트 관리
        void UpdateHeartbeat() noexcept;
        bool IsExpired(std::chrono::minutes timeout = std::chrono::minutes{30}) const noexcept;

        // 이벤트 핸들러
        virtual void OnConnected();
        virtual void OnDisconnected();
        virtual void OnRecvData(const std::uint8_t *buffer, std::size_t len);

        // 데이터 전송
        task<void> SendData(std::vector<std::uint8_t> &&buffer);

    private:
        std::unique_ptr<socket_client> client_;
        std::string session_id_;
        PlayerData player_data_;
        std::chrono::steady_clock::time_point last_heartbeat_;
        std::atomic<bool> connected_{false};
    };

    // 전역 세션 관리자
    class SessionManager
    {
    public:
        static SessionManager &GetInstance() noexcept;

        // 세션 관리
        std::string AddSession(std::unique_ptr<socket_client> client);
        std::shared_ptr<GameSession> GetSession(const std::string &session_id);
        void RemoveSession(const std::string &session_id);

        // 정리 작업
        void CleanupExpiredSessions();
        std::size_t GetActiveSessionCount() const noexcept;

        // 세션 처리 코루틴
        task<void> HandleSession(std::shared_ptr<GameSession> session);

    private:
        SessionManager() = default;
        std::string GenerateSessionId();

        mutable std::shared_mutex mutex_;
        std::unordered_map<std::string, std::shared_ptr<GameSession>> sessions_;
    };

    // 세션 핸들러 코루틴 함수
    task<void> HandleClientSession(std::unique_ptr<socket_client> client);

} // namespace co_uring