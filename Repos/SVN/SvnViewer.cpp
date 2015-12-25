#include "SvnViewer.h"
#include <unistd.h>

SvnViewer* SvnViewer::instance()
{
    static SvnViewer* pInstance = new SvnViewer();
    return pInstance;
}

SvnViewer::SvnViewer()
    : m_observer(nullptr)
{
    m_nRevisionsCount = 2500;
    m_currentRevision = -1;
    m_closing = false;
}

SvnViewer::~SvnViewer()
{
    m_closing = true;
    {
        std::unique_lock<std::recursive_mutex> locker(m_mutex);
        for(threadParams* thread : m_asyncProcessThreads)
        {
            pthread_join(thread->thread, NULL);
            delete thread;
        }
    }
}

bool SvnViewer::isInitialized()
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    return !m_repoPath.empty();
}

void SvnViewer::init(const std::string& repoPath, int nRevisionsCount)
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    m_repoPath = repoPath;
    m_nRevisionsCount = nRevisionsCount;

    m_repoContent.reset(new RepoItemInfo(nullptr, m_repoPath, RepoItemInfo::Directory));

    refresh();
}

void SvnViewer::refresh()
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    launchAsync(new InfoSvnCommand(m_repoPath.c_str()));
}

void SvnViewer::viewLog(const std::string &repoUrl)
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    launchAsync(new LogSvnCommand(repoUrl, m_nRevisionsCount));
}

void SvnViewer::updateToHead()
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    launchAsync(new UpdateSvnCommand(m_repoPath));
}

void SvnViewer::updateToRevision(int nRevision)
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    launchAsync(new UpdateSvnCommand(m_repoPath, nRevision));
}

void SvnViewer::checkForModifications()
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    launchAsync(new StatusSvnCommand(m_repoPath));
}

void SvnViewer::commit(const std::list<std::string>& items, const std::string& message)
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    launchAsync(new CommitSvnCommand(m_repoPath, items, message));
}

void SvnViewer::launchDiffViewer(const std::string& strItem, int nRevision)
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    launchAsync(new LaunchDiffViewerSvnCommand(strItem, nRevision));
}

void SvnViewer::addToSourceControl(const std::string& strItem)
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    launchAsync(new AddSvnCommand(m_repoPath, strItem));
}

void SvnViewer::revert(const std::string& strItem)
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    launchAsync(new RevertSvnCommand(m_repoPath, strItem));
}

void SvnViewer::listContent(const std::string& repoPath)
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);

    RepoItemInfo::SmartPtr node = findRepoNode(repoPath);
    if(node)
    {
        launchAsync(new ListSvnCommand(repoPath, node));
    }
}

bool SvnViewer::getChangeSet(int nRevision, RevisionInfo& changeset)
{
   std::unique_lock<std::recursive_mutex> locker(m_mutex);
   RevisionInfo::Collection::iterator currentRevision = m_revisionsList.begin();
   for(RevisionInfo::Collection::iterator revIt = m_revisionsList.begin(); revIt != m_revisionsList.end(); ++revIt)
   {
       if(revIt->m_No==nRevision)
       {
           currentRevision = revIt;
       }
   }

   if(currentRevision == m_revisionsList.end())
   {
       std::stringstream ss; ss << nRevision;
       std::string error = std::string("Received an invalid revision number <")  + ss.str() + "> while trying to obtain changeset information.";
       m_errors.push_back(error);
       return false;
   }

   changeset = *currentRevision;
   if(currentRevision->m_AffectedItems.empty() && !currentRevision->m_bLoading)
   {
       currentRevision->m_bLoading = true;
       launchAsync(new DiffSvnCommand(m_repoUrl, nRevision));
   }

   return true;
}

RevisionInfo::Collection SvnViewer::getRevisionsList() const
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    return m_revisionsList;
}

ChangeInfo::Collection SvnViewer::getLocalChanges() const
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    return m_localChanges;
}

int SvnViewer::getCurrentRevision() const
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    return m_currentRevision;
}

std::string SvnViewer::getRepoPath() const
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    return m_repoPath;
}

RepoItemInfo::SmartPtr SvnViewer::getRepoContent() const
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    return m_repoContent;
}

RepoItemInfo::SmartPtr SvnViewer::findRepoNode(const std::string& repoPath)
{
    if(m_repoContent->m_name == repoPath)
        return m_repoContent;

    return m_repoContent->findChildNode(repoPath);
}

void SvnViewer::launchAsync(SvnCommand* pCommand)
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);

    threadParams *pParams = new threadParams;
    pParams->pInstance = this;
    pParams->spCommand.reset(pCommand);
    pthread_create(&pParams->thread, NULL, &asyncProcessThread, pParams);
    m_asyncProcessThreads.push_back(pParams);
}

void SvnViewer::onAsyncCommandCompleted(threadParams *pParams, bool bSuccess)
{
    std::unique_lock<std::recursive_mutex> locker(m_mutex);
    if(bSuccess)
    {
        if(pParams->spCommand->getType() == "svn diff")
        {
            DiffSvnCommand* pCommand = static_cast<DiffSvnCommand*>(pParams->spCommand.get());
            if(pCommand->getAffectedItems().size())
            {
                for(RevisionInfo::Collection::iterator revIt = m_revisionsList.begin();revIt != m_revisionsList.end(); ++revIt)
                {
                    if(revIt->m_No == pCommand->getRevision())
                    {
                        revIt->m_bLoading = false;
                        revIt->m_AffectedItems = pCommand->getAffectedItems();
                        break;
                    }
                }

                m_observer->onAffectedItemsUpdated();
            }
        }
        else
        if(pParams->spCommand->getType() == "svn log")
        {
            LogSvnCommand* pCommand = static_cast<LogSvnCommand*>(pParams->spCommand.get());
            RevisionInfo::Collection oldRevisions;
            m_revisionsList.swap(oldRevisions);
            m_revisionsList = pCommand->getRevisions();

            for(RevisionInfo& oldRev : oldRevisions)
            {
                for(RevisionInfo& newRev : m_revisionsList)
                {
                    //save affected items
                    if(newRev.m_No == oldRev.m_No)
                    {
                        newRev = oldRev;
                        break;
                    }
                }
            }

            m_observer->onRevisionsListUpdated();
        }
        else
        if(pParams->spCommand->getType() == "svn status")
        {
            StatusSvnCommand* pCommand = static_cast<StatusSvnCommand*>(pParams->spCommand.get());
            m_localChanges = pCommand->getChanges();

            m_observer->onLocalModificationsUpdated();
        }
        else
        if(pParams->spCommand->getType() == "svn info")
        {
            InfoSvnCommand* pCommand = static_cast<InfoSvnCommand*>(pParams->spCommand.get());
            m_repoUrl = pCommand->getRepoUrl();
            m_currentRevision = pCommand->getCurrentRevision();

            listContent(m_repoPath);
            viewLog(m_repoUrl);
            checkForModifications();
        }
        else
        if(pParams->spCommand->getType() == "svn update")
        {
            refresh();
        }
        else
        if(pParams->spCommand->getType() == "svn add")
        {
            refresh();
        }
        else
        if(pParams->spCommand->getType() == "svn revert")
        {
            refresh();
        }
        else
        if(pParams->spCommand->getType() == "svn commit")
        {
            refresh();
        }
        else
        if(pParams->spCommand->getType() == "svn diff --diff-cmd")
        {
            //nothing to do
        }
        else
        if(pParams->spCommand->getType() == "svn list")
        {
            m_observer->onRepoContentUpdated();
        }
    }
    else
    {
        m_observer->onErrosGenerated();
    }

    for(std::list<threadParams*>::iterator threadIt = m_asyncProcessThreads.begin(); threadIt != m_asyncProcessThreads.end(); ++threadIt)
    {
        if(*threadIt == pParams)
        {
            m_asyncProcessThreads.erase(threadIt);
            break;
        }
    }
}

void* SvnViewer::asyncProcessThread(void* arg)
{
    threadParams* pParams(reinterpret_cast<threadParams*>(arg));

    bool bSuccess = pParams->spCommand->execute();
    pParams->pInstance->onAsyncCommandCompleted(pParams, bSuccess);

    return NULL;
}
