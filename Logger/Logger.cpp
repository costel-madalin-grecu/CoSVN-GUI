#include "Logger/Logger.h"
#include "Settings/AppSettings.h"

Logger::Logger()
{
    m_commandsLogFile = AppSettings::instance()->getLogsPath() + "commands.log";

    //truncate old logs
    system((std::string(">") + m_commandsLogFile).c_str());
}

void Logger::logCommandMessage(const std::string& message)
{
    system((std::string("echo \"") + message + std::string("\">>") + m_commandsLogFile).c_str());
}

Logger* Logger::instance()
{
    static Logger* logger = new Logger();
    return logger;
}
