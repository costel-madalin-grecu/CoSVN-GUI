#ifndef APPSETTINGS_H
#define APPSETTINGS_H

#include <string>
#include <list>
#include <mutex>

class AppSettings
{
protected:
    AppSettings();

public:
    static AppSettings* instance();

    std::list<std::string> getRecents();
    void addToHistory(const std::string& repoPath);

    std::string getLastRepoPath();
    std::string getSettingsPath();
    std::string getTempPath();
    std::string getLogsPath();

private:
    std::recursive_mutex m_mutex;
    std::string m_lastRepoPath;
    std::string m_path;
    std::list<std::string> m_recents;
};

#endif // APPSETTINGS_H
