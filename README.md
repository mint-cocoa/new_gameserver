# 고성능 게임서버 (Co_uring GameServer)

Linux io_uring과 C++20 코루틴을 사용한 고성능 게임서버 구현체입니다.

## 주요 특징

- **io_uring 기반**: Linux io_uring을 활용한 비동기 I/O 처리
- **C++20 코루틴**: 코루틴을 사용한 비동기 프로그래밍
- **멀티 워커**: 멀티스레드 워커 아키텍처
- **세션 관리**: 클라이언트 세션 관리 시스템
- **링 버퍼**: 효율적인 메모리 관리를 위한 링 버퍼 구현

## 프로젝트 구조

```
new_gameserver/
├── server/           # 서버 코어 로직
├── io/              # I/O 관련 모듈 (socket, io_uring, buffer_ring)
├── coroutine/       # 코루틴 태스크 관리
├── session/         # 세션 관리
├── client/          # 테스트 클라이언트
├── logs/            # 로그 파일
└── build/           # 빌드 출력물
```

## 빌드 및 실행

### 필요 조건

- Linux (io_uring 지원)
- GCC 11+ 또는 Clang 12+ (C++20 지원)
- CMake 3.20+
- liburing

### 빌드

```bash
# 의존성 설치 (Ubuntu/Debian)
sudo apt install liburing-dev cmake build-essential

# 빌드
mkdir build
cd build
cmake ..
make

# 실행
./gameserver
```

### 테스트 클라이언트

```bash
cd client
make
./simple_client
```

## 아키텍처

### 서버 구조

1. **GameServer**: 메인 서버 클래스, 워커 스레드 관리
2. **Worker**: 개별 워커 스레드, 클라이언트 연결 처리
3. **SessionManager**: 클라이언트 세션 관리

### I/O 시스템

- **io_uring**: 비동기 I/O 처리
- **socket**: TCP 소켓 래퍼
- **buffer_ring**: 효율적인 메모리 버퍼 관리

### 코루틴 시스템

- **task**: 코루틴 태스크 템플릿
- **spawn**: 코루틴 스폰 유틸리티

## 사용법

```cpp
#include "server/include/server.h"

int main() {
    co_uring::GameServer server(4); // 4개 워커 스레드
    
    if (!server.start("127.0.0.1", 8080)) {
        return -1;
    }
    
    server.wait_for_shutdown();
    return 0;
}
```

## 성능 특징

- io_uring을 통한 Zero-copy I/O
- 코루틴 기반 비동기 처리로 높은 동시성
- 멀티 워커를 통한 CPU 코어 활용
- 링 버퍼를 통한 메모리 효율성

## 라이선스

MIT License

## 기여

Pull Request와 Issue를 환영합니다. 