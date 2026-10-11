#pragma once

#include <QString>
#include <memory>

class WindowsMediaSession final {
public:
    enum class PlaybackState { Stopped, Playing, Paused };
    enum class Button : unsigned int { Play = 0, Pause = 1, Stop = 2, Next = 6, Previous = 7 };

    // WM_APP + 0x470. The WinRT callback posts this to the owning Qt window.
    static constexpr unsigned int ButtonMessage = 0x8470;

    explicit WindowsMediaSession(void *windowHandle);
    ~WindowsMediaSession();
    WindowsMediaSession(const WindowsMediaSession &) = delete;
    WindowsMediaSession &operator=(const WindowsMediaSession &) = delete;

    bool isAvailable() const;
    void setTrack(const QString &title, const QString &artist);
    void setPlaybackState(PlaybackState state);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
