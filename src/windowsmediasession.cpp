#include "windowsmediasession.h"

#ifdef Q_OS_WIN

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <roapi.h>
#include <systemmediatransportcontrolsinterop.h>
#include <winstring.h>

#include <QDebug>
#include <atomic>

// The Qt MinGW headers omit these Windows.Media WinRT interfaces. Keep the
// ABI declarations and IIDs in sync with the Windows SDK's windows.media.h.
namespace Media {
struct EventRegistrationToken { LONGLONG value; };
enum MediaPlaybackStatus : int { MediaPlaybackStatus_Stopped = 2,
                                 MediaPlaybackStatus_Playing = 3,
                                 MediaPlaybackStatus_Paused = 4 };
enum MediaPlaybackType : int { MediaPlaybackType_Music = 1 };
enum SystemMediaTransportControlsButton : int {
    SystemMediaTransportControlsButton_Play = 0,
    SystemMediaTransportControlsButton_Pause = 1,
    SystemMediaTransportControlsButton_Stop = 2,
    SystemMediaTransportControlsButton_Next = 6,
    SystemMediaTransportControlsButton_Previous = 7
};
struct ISystemMediaTransportControls;
struct IMusicDisplayProperties : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Title(HSTRING*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Title(HSTRING) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AlbumArtist(HSTRING*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AlbumArtist(HSTRING) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Artist(HSTRING*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Artist(HSTRING) = 0;
};
struct ISystemMediaTransportControlsButtonPressedEventArgs : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Button(SystemMediaTransportControlsButton*) = 0;
};
struct ButtonHandlerInterface : IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Invoke(ISystemMediaTransportControls*,
        ISystemMediaTransportControlsButtonPressedEventArgs*) = 0;
};
struct ISystemMediaTransportControlsDisplayUpdater : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_Type(MediaPlaybackType*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Type(MediaPlaybackType) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_AppMediaId(HSTRING*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_AppMediaId(HSTRING) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_Thumbnail(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_Thumbnail(void*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_MusicProperties(IMusicDisplayProperties**) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_VideoProperties(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_ImageProperties(void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE CopyFromFileAsync(MediaPlaybackType, void*, void**) = 0;
    virtual HRESULT STDMETHODCALLTYPE ClearAll() = 0;
    virtual HRESULT STDMETHODCALLTYPE Update() = 0;
};
struct ISystemMediaTransportControls : IInspectable {
    virtual HRESULT STDMETHODCALLTYPE get_PlaybackStatus(MediaPlaybackStatus*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_PlaybackStatus(MediaPlaybackStatus) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_DisplayUpdater(ISystemMediaTransportControlsDisplayUpdater**) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_SoundLevel(int*) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsEnabled(boolean*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsEnabled(boolean) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsPlayEnabled(boolean*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsPlayEnabled(boolean) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsStopEnabled(boolean*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsStopEnabled(boolean) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsPauseEnabled(boolean*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsPauseEnabled(boolean) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsRecordEnabled(boolean*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsRecordEnabled(boolean) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsFastForwardEnabled(boolean*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsFastForwardEnabled(boolean) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsRewindEnabled(boolean*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsRewindEnabled(boolean) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsPreviousEnabled(boolean*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsPreviousEnabled(boolean) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsNextEnabled(boolean*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsNextEnabled(boolean) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsChannelUpEnabled(boolean*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsChannelUpEnabled(boolean) = 0;
    virtual HRESULT STDMETHODCALLTYPE get_IsChannelDownEnabled(boolean*) = 0;
    virtual HRESULT STDMETHODCALLTYPE put_IsChannelDownEnabled(boolean) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_ButtonPressed(ButtonHandlerInterface*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_ButtonPressed(EventRegistrationToken) = 0;
    virtual HRESULT STDMETHODCALLTYPE add_PropertyChanged(void*, EventRegistrationToken*) = 0;
    virtual HRESULT STDMETHODCALLTYPE remove_PropertyChanged(EventRegistrationToken) = 0;
};
constexpr GUID ControlsIID = {0x99fa3ff4, 0x1742, 0x42a6,
    {0x90, 0x2e, 0x08, 0x7d, 0x41, 0xf9, 0x65, 0xec}};
constexpr GUID ButtonHandlerIID = {0x0557e996, 0x7b23, 0x5bae,
    {0xaa, 0x81, 0xea, 0x0d, 0x67, 0x11, 0x43, 0xa4}};
} // namespace Media

namespace {
using ButtonHandlerInterface = Media::ButtonHandlerInterface;

class ButtonHandler final : public ButtonHandlerInterface {
public:
    explicit ButtonHandler(HWND window) : m_window(window) {}

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **result) override
    {
        if (!result) return E_POINTER;
        *result = nullptr;
        if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, Media::ButtonHandlerIID)) {
            *result = static_cast<ButtonHandlerInterface*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    ULONG STDMETHODCALLTYPE AddRef() override { return ++m_refs; }
    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG remaining = --m_refs;
        if (!remaining) delete this;
        return remaining;
    }

    HRESULT STDMETHODCALLTYPE Invoke(Media::ISystemMediaTransportControls*,
                                     Media::ISystemMediaTransportControlsButtonPressedEventArgs *args) override
    {
        if (!args) return E_POINTER;
        Media::SystemMediaTransportControlsButton button;
        const HRESULT result = args->get_Button(&button);
        if (SUCCEEDED(result))
            PostMessageW(m_window, WindowsMediaSession::ButtonMessage,
                         static_cast<WPARAM>(button), 0);
        return result;
    }

private:
    std::atomic<ULONG> m_refs {1};
    HWND m_window = nullptr;
};

HSTRING toHString(const QString &text)
{
    HSTRING value = nullptr;
    WindowsCreateString(reinterpret_cast<PCWSTR>(text.utf16()),
                        static_cast<UINT32>(text.size()), &value);
    return value;
}
} // namespace

struct WindowsMediaSession::Impl {
    bool initializedRuntime = false;
    Media::ISystemMediaTransportControls *controls = nullptr;
    Media::ISystemMediaTransportControlsDisplayUpdater *updater = nullptr;
    ButtonHandler *handler = nullptr;
    Media::EventRegistrationToken buttonToken {};
    bool buttonSubscribed = false;
    QString title;
    QString artist;

    ~Impl()
    {
        if (controls) {
            if (buttonSubscribed) controls->remove_ButtonPressed(buttonToken);
            controls->put_IsEnabled(false);
        }
        if (handler) handler->Release();
        if (updater) updater->Release();
        if (controls) controls->Release();
        if (initializedRuntime) RoUninitialize();
    }
};

WindowsMediaSession::WindowsMediaSession(void *windowHandle)
    : m_impl(std::make_unique<Impl>())
{
    const HWND window = static_cast<HWND>(windowHandle);
    if (!window || !IsWindow(window)) return;

    const HRESULT initResult = RoInitialize(RO_INIT_SINGLETHREADED);
    m_impl->initializedRuntime = SUCCEEDED(initResult);
    if (FAILED(initResult) && initResult != RPC_E_CHANGED_MODE) {
        qWarning() << "Windows media controls initialization failed:" << Qt::hex << initResult;
        return;
    }

    HSTRING className = toHString(QStringLiteral("Windows.Media.SystemMediaTransportControls"));
    if (!className) return;
    ISystemMediaTransportControlsInterop *interop = nullptr;
    HRESULT result = RoGetActivationFactory(
        className, __uuidof(ISystemMediaTransportControlsInterop),
        reinterpret_cast<void**>(&interop));
    WindowsDeleteString(className);
    if (FAILED(result) || !interop) {
        qWarning() << "Windows media controls factory unavailable:" << Qt::hex << result;
        return;
    }

    result = interop->GetForWindow(window, Media::ControlsIID,
                                   reinterpret_cast<void**>(&m_impl->controls));
    interop->Release();
    if (FAILED(result) || !m_impl->controls) {
        qWarning() << "Windows media controls unavailable for window:" << Qt::hex << result;
        return;
    }

    result = m_impl->controls->get_DisplayUpdater(&m_impl->updater);
    if (FAILED(result) || !m_impl->updater) {
        qWarning() << "Windows media display updater unavailable:" << Qt::hex << result;
        return;
    }

    m_impl->handler = new ButtonHandler(window);
    result = m_impl->controls->add_ButtonPressed(m_impl->handler, &m_impl->buttonToken);
    m_impl->buttonSubscribed = SUCCEEDED(result);
    if (FAILED(result))
        qWarning() << "Windows media buttons unavailable:" << Qt::hex << result;

    m_impl->controls->put_IsPlayEnabled(true);
    m_impl->controls->put_IsPauseEnabled(true);
    m_impl->controls->put_IsStopEnabled(true);
    m_impl->controls->put_IsPreviousEnabled(true);
    m_impl->controls->put_IsNextEnabled(true);
}

WindowsMediaSession::~WindowsMediaSession() = default;

bool WindowsMediaSession::isAvailable() const
{
    return m_impl->controls && m_impl->updater;
}

void WindowsMediaSession::setTrack(const QString &title, const QString &artist)
{
    if (!isAvailable()) return;
    if (title.isEmpty()) {
        m_impl->title.clear();
        m_impl->artist.clear();
        m_impl->controls->put_PlaybackStatus(Media::MediaPlaybackStatus_Stopped);
        m_impl->controls->put_IsEnabled(false);
        return;
    }
    if (m_impl->title == title && m_impl->artist == artist) return;

    m_impl->updater->ClearAll();
    m_impl->updater->put_Type(Media::MediaPlaybackType_Music);
    Media::IMusicDisplayProperties *music = nullptr;
    if (FAILED(m_impl->updater->get_MusicProperties(&music)) || !music) return;
    HSTRING titleValue = toHString(title);
    HSTRING artistValue = toHString(artist);
    const HRESULT titleResult = music->put_Title(titleValue);
    const HRESULT artistResult = music->put_Artist(artistValue);
    WindowsDeleteString(titleValue);
    WindowsDeleteString(artistValue);
    music->Release();
    if (FAILED(titleResult) || FAILED(artistResult) || FAILED(m_impl->updater->Update()))
        return;

    m_impl->title = title;
    m_impl->artist = artist;
    m_impl->controls->put_IsEnabled(true);
}

void WindowsMediaSession::setPlaybackState(PlaybackState state)
{
    if (!isAvailable() || m_impl->title.isEmpty()) return;
    Media::MediaPlaybackStatus status = Media::MediaPlaybackStatus_Stopped;
    if (state == PlaybackState::Playing) status = Media::MediaPlaybackStatus_Playing;
    else if (state == PlaybackState::Paused) status = Media::MediaPlaybackStatus_Paused;
    m_impl->controls->put_PlaybackStatus(status);
}

#else

struct WindowsMediaSession::Impl {};
WindowsMediaSession::WindowsMediaSession(void *) : m_impl(std::make_unique<Impl>()) {}
WindowsMediaSession::~WindowsMediaSession() = default;
bool WindowsMediaSession::isAvailable() const { return false; }
void WindowsMediaSession::setTrack(const QString &, const QString &) {}
void WindowsMediaSession::setPlaybackState(PlaybackState) {}

#endif
