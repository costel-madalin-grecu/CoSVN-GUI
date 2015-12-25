#include "AppSettings.h"

#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <fstream>
#include <QString>
#include "stdlib.h"

AppSettings::AppSettings()
{
    struct passwd *pw = getpwuid(getuid());
    QString homeDir(pw->pw_dir);
    QString settingsFile = homeDir + QString(homeDir.endsWith("/") ? "" : "/");
    m_path = (settingsFile + ".CoSvn/").toStdString();
    system((QString("mkdir \"") + m_path.c_str() + "\"").toStdString().c_str());
    system((QString("mkdir \"") + m_path.c_str() + "Temp/\"").toStdString().c_str());
    system((QString("mkdir \"") + m_path.c_str() + "Logs/\"").toStdString().c_str());

    std::string recentsPath = m_path + "recentPaths";
    std::ifstream fStream(recentsPath.c_str(), std::ios_base::in);
    std::string lastRepo;
    while(getline(fStream, lastRepo, '\n'))
    {
        if(m_lastRepoPath.empty())
        {
            m_lastRepoPath = lastRepo;
        }

        m_recents.push_back(lastRepo);
    }
}


AppSettings* AppSettings::instance()
{
    static AppSettings* appSettings = new AppSettings();
    return appSettings;
}

std::list<std::string> AppSettings::getRecents()
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    return m_recents;
}

void AppSettings::addToHistory(const std::string &repoPath)
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);

    if(m_lastRepoPath == repoPath)
    {
        return;
    }

    m_lastRepoPath = repoPath;
    for(std::list<std::string>::iterator it = m_recents.begin(); it != m_recents.end(); ++it)
    {
       if(*it == repoPath)
       {
           m_recents.erase(it);
           break;
       }
    }
    m_recents.push_front(repoPath);

    std::string recentsPath = m_path + "recentPaths";
    system((std::string(">") + recentsPath).c_str());
    for(auto item : m_recents)
    {
       system((std::string("echo \"") + item + "\">>" + recentsPath).c_str());
    }
}

std::string AppSettings::getLastRepoPath()
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    return m_lastRepoPath;
}

std::string AppSettings::getSettingsPath()
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    return m_path;
}

std::string AppSettings::getTempPath()
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    return m_path + "Temp/";
}

std::string AppSettings::getLogsPath()
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    return m_path + "Logs/";
}
