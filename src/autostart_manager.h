#pragma once

#include <QString>

class AutostartManager final
{
public:
    static QString autostartFilePath();
    static bool isEnabled();
    static bool setEnabled(bool enabled, QString *errorMessage = nullptr);
};
