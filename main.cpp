#include <QApplication>
#include <QByteArray>
#include <QIcon>
#include <QSettings>
#include "ui_widget.h"

int main(int argc, char* argv[]) {
    // VS Code Snap terminals may inject GTK variables that force mixed
    // snap/system runtime libraries and crash Qt apps on startup.
    auto clearIfSnapPath = [](const char* name) {
        if (!qEnvironmentVariableIsSet(name)) return;
        const QByteArray value = qgetenv(name);
        if (value.contains("/snap/")) {
            qunsetenv(name);
        }
    };
    clearIfSnapPath("GTK_EXE_PREFIX");
    clearIfSnapPath("GTK_PATH");
    clearIfSnapPath("GTK_IM_MODULE_FILE");

    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName("InfinityAudio");
    app.setOrganizationName("InfinityAudio");
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/packaging/icons/infinityaudio.ico")));

    UiWidget widget;
    widget.setWindowIcon(app.windowIcon());
    widget.show();

    return app.exec();
}
