#ifndef SVNVIEWER_H
#define SVNVIEWER_H

#include <sstream>
#include <string>
#include <list>
#include <memory>
#include <mutex>
#include <chrono>
#include <thread>
#include <pthread.h>
#include <iostream>
#include "Repos/SVN/SvnCommands.h"

class SvnViewerObserver
{
public:
    virtual void onRevisionsListUpdated() = 0;
    virtual void onLocalModificationsUpdated() = 0;
    virtual void onAffectedItemsUpdated() = 0;
    virtual void onRepoContentUpdated() = 0;
    virtual void onErrosGenerated() = 0;
};

class SvnViewer
{
private:
    SvnViewer();
    ~SvnViewer();

public:
    static SvnViewer* instance();

    void setObserver(SvnViewerObserver* observer)
    {
        m_observer = observer;
    }

    bool isInitialized();

    void init(const std::string& repoPath, int nRevisionsCount = 2500);
    void refresh();
    void viewLog(const std::string& repoUrl);
    void updateToHead();
    void updateToRevision(int nRevision);
    void checkForModifications();
    void commit(const std::list<std::string>& items, const std::string& message);
    void launchDiffViewer(const std::string& strItem, int nRevision);
    void addToSourceControl(const std::string& strItem);
    void revert(const std::string& strItem);
    void listContent(const std::string& repoPath);

    bool getChangeSet(int nRevision, RevisionInfo& changeset);
    RevisionInfo::Collection getRevisionsList() const;
    ChangeInfo::Collection getLocalChanges() const;
    int getCurrentRevision() const;
    std::string getRepoPath() const;
    RepoItemInfo::SmartPtr getRepoContent() const;
private:

    RepoItemInfo::SmartPtr findRepoNode(const std::string& repoPath);
    void launchAsync(SvnCommand* pCommand);

    struct threadParams
    {
        pthread_t thread;
        std::unique_ptr<SvnCommand> spCommand;
        SvnViewer* pInstance;
    };


    void onAsyncCommandCompleted(threadParams* pParams, bool bSuccess);
    static void* asyncProcessThread(void* arg);

private:

    mutable std::recursive_mutex m_mutex;

    bool m_closing;
    std::list<threadParams*> m_asyncProcessThreads;

    int m_nRevisionsCount;
    int m_currentRevision;
    std::string m_repoPath;
    std::string m_repoUrl;
    SvnViewerObserver* m_observer;

    RevisionInfo::Collection m_revisionsList;
    ChangeInfo::Collection m_localChanges;
    std::list<std::string> m_errors;

    RepoItemInfo::SmartPtr m_repoContent;
};

#endif // SVNVIEWER_H
