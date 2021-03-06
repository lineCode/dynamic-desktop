#pragma once

#include <Windows.h>

#include <QWidget>

QT_BEGIN_NAMESPACE
QT_FORWARD_DECLARE_CLASS(QFileInfo)
QT_END_NAMESPACE

namespace Utils
{
    static HWND HWORKERW = nullptr;
    static HANDLE mutex = nullptr;

    HWND getProgman();
    HWND getWorkerW(bool legacyMode);
    void Exit();
    void Exit(int resultCode);
    int ExitProgram(int resultCode);
    void fileLogger(QtMsgType type, const QMessageLogContext &context, const QString &msg);
    QStringList externalFilesToLoad(const QFileInfo &originalMediaFile, const QString &fileType);
    void moveToCenter(QWidget *window);
}
