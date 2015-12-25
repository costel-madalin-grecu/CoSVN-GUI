#ifndef LOGGER_H
#define LOGGER_H

#include <string>

class Logger
{
private:
    Logger();

public:
    static Logger* instance();

public:
    void logCommandMessage(const std::string& message);

private:
    std::string m_commandsLogFile;
};

#endif // LOGGER_H
