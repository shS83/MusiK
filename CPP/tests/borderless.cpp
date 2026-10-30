// Compile the real Visual widget, including its native F11 handler.
#define main musik_application_main
#include "../src/main.cpp"
#undef main
#include <QEventLoop>
#include <cstdlib>

static void check(bool ok, const char *message) {
    if (!ok) { qCritical("%s", message); std::exit(1); }
}
static void settle() {
    QEventLoop loop;
    QTimer::singleShot(80, &loop, &QEventLoop::quit);
    loop.exec();
}
static void key(Visual &v, int code) {
    QKeyEvent event(QEvent::KeyPress, code, Qt::NoModifier);
    QApplication::sendEvent(&v, &event);
    settle();
}
int main(int argc, char **argv) {
    QSurfaceFormat format;
    format.setVersion(3,3); format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc, argv);
    Visual visual;
    visual.resize(640,360); visual.show(); settle();
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(visual.winId());
    auto *context = visual.context();
    RECT original{}; GetWindowRect(hwnd, &original);
    auto style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    auto exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
    MONITORINFOEXW info{}; info.cbSize = sizeof(info);
    check(GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &info), "monitor");
    DEVMODEW before{}; before.dmSize=sizeof(before);
    check(EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &before), "display mode");
    for (int i=0; i<3; ++i) {
        key(visual, Qt::Key_F11);
        check(reinterpret_cast<HWND>(visual.winId()) == hwnd, "HWND changed");
        check(visual.context() == context, "OpenGL context changed");
        check(!(GetWindowLongPtrW(hwnd,GWL_STYLE) & (WS_CAPTION|WS_THICKFRAME)), "borders remain");
        check(GetWindowLongPtrW(hwnd,GWL_EXSTYLE) & WS_EX_LAYERED, "composition guard missing");
        BYTE alpha=0; DWORD flags=0; COLORREF color=0;
        check(GetLayeredWindowAttributes(hwnd,&color,&alpha,&flags) && alpha==254 && (flags&LWA_ALPHA), "blend missing");
        RECT full{}; GetWindowRect(hwnd,&full);
        check(EqualRect(&full,&info.rcMonitor), "wrong monitor coverage");
        check(!visual.grabFramebuffer().isNull(), "empty OpenGL framebuffer");
        DEVMODEW after{}; after.dmSize=sizeof(after);
        check(EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &after), "display mode after");
        check(before.dmPelsWidth==after.dmPelsWidth && before.dmPelsHeight==after.dmPelsHeight && before.dmDisplayFrequency==after.dmDisplayFrequency && before.dmBitsPerPel==after.dmBitsPerPel, "display mode changed");
        key(visual, i%2 ? Qt::Key_F11 : Qt::Key_Escape);
        RECT restored{}; GetWindowRect(hwnd,&restored);
        check(EqualRect(&original,&restored), "geometry not restored");
        check(GetWindowLongPtrW(hwnd,GWL_STYLE)==style && GetWindowLongPtrW(hwnd,GWL_EXSTYLE)==exStyle, "styles not restored");
    }
    visual.showMaximized(); settle();
    RECT maximized{}; GetWindowRect(hwnd,&maximized);
    key(visual,Qt::Key_F11); key(visual,Qt::Key_F11);
    RECT restored{}; GetWindowRect(hwnd,&restored);
    check(EqualRect(&maximized,&restored), "maximized geometry not restored");
#endif
    key(visual, Qt::Key_Escape);
    check(!visual.isVisible(), "ESC did not close window");
    qInfo("Borderless regression passed");
    return 0;
}
