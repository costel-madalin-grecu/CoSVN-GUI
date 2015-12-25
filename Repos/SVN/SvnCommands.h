#ifndef SVNCOMMANDS_H
#define SVNCOMMANDS_H

#include "Logger/Logger.h"
#include "Settings/AppSettings.h"

#include <unistd.h>
#include <memory>

static bool stringEndsWith(const std::string& str, char c)
{
    if(str.empty())
        return false;

    if(str[str.length() - 1] == c)
        return true;

    return false;
}

class RepoItemInfo
{
public:
    typedef std::shared_ptr<RepoItemInfo> SmartPtr;
    typedef std::list<SmartPtr> Collection;
    enum ItemType
    {
        File,
        Directory
    };

    RepoItemInfo()
    {
        m_type = File;
        m_parent = nullptr;
    }

    RepoItemInfo(RepoItemInfo* parent, const std::string& name, ItemType type = File)
    {
        m_type = type;
        m_name = name;
        m_parent = parent;
    }

    const std::string getFullPath() const
    {
        std::string fullPath = m_name;
        RepoItemInfo* parent = m_parent;
        while (parent)
        {
            fullPath = parent->m_name + (stringEndsWith(parent->m_name, '/') ? "" : "/") + fullPath;
            parent = parent->m_parent;
        }

        return fullPath;
    }

    RepoItemInfo::SmartPtr findChildNode(const std::string& fullPath)
    {
        RepoItemInfo::SmartPtr result;
        for(RepoItemInfo::SmartPtr child : m_subItems)
        {
            const std::string childFullPath = child->getFullPath();
            if(fullPath == childFullPath)
            {
                result = child;
                break;
            }

            result = child->findChildNode(fullPath);
            if(result)
            {
                break;
            }
        }

        return result;
    }

public:
    ItemType m_type;
    std::string m_name;
    RepoItemInfo* m_parent;
    RepoItemInfo::Collection m_subItems;
};

class RevisionInfo
{
public:
    typedef std::list<RevisionInfo> Collection;
    RevisionInfo() : m_bLoading(false)
    {
    }

public:
    bool m_bLoading;
    int m_No;
    std::string m_Description;
    std::string m_Author;
    std::string m_Date;
    std::list<std::string> m_AffectedItems;
};

class ChangeInfo
{
public:
    typedef std::list<ChangeInfo> Collection;

    std::string m_AffectedItem;
    std::string m_Status;
};

class SvnCommand
{
public:
    SvnCommand(const std::string& repoPath)
        : m_path(repoPath)
    {
    }

    virtual std::string getType() const = 0;
    virtual bool execute() = 0;

    static std::string executeShellCommand(const std::string& cmd)
    {
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe)
        {
            Logger::instance()->logCommandMessage(std::string("Failed to open pipe."));
            return "";
        }

        char buffer[128];
        std::string result = "";
        while(!feof(pipe))
        {
            if(fgets(buffer, 128, pipe) != NULL)
            {
                result += buffer;
            }
        }
        pclose(pipe);
        Logger::instance()->logCommandMessage(std::string("========================================\nExecuting command:\n")
                                              + cmd + "." + std::string("Obtained result:\n") + result + "\n");
        return result;
    }

protected:
    std::string m_path;
};

class LogSvnCommand : public SvnCommand
{
public:
    LogSvnCommand(const std::string& path, int nRevisions)
        : SvnCommand(path)
        , m_nDisplayRevisionsCount(nRevisions)
    {
    }

    virtual std::string getType() const { return "svn log"; }
    virtual bool execute()
    {
        std::stringstream ss; ss << m_nDisplayRevisionsCount;
        std::string result = executeShellCommand(std::string("svn log ") + m_path + " -l " + ss.str());

        if(result.empty())
            return false;

        std::istringstream stringStream(result);

        RevisionInfo revision;
        int nRevisionLine = 0;
        std::string revisionPart;
        int nCommentLines = 1;
        while(getline(stringStream, revisionPart, '\n'))
        {
            switch(nRevisionLine)
            {
                case 1:
                    {
                        if(revisionPart[0] != 'r')
                        {
                            return false;
                        }

                        size_t nPos = revisionPart.find("|");
                        if(nPos == std::string::npos)
                        {
                            return false;
                        }

                        revision.m_No = atoi(revisionPart.substr(1, nPos).c_str());

                        size_t nStart = nPos + 2;
                        nPos = revisionPart.find("|", nStart);
                        if(nPos == std::string::npos)
                        {
                            return false;
                        }

                        revision.m_Author = revisionPart.substr(nStart, nPos - nStart - 1);

                        nStart = nPos + 2;
                        nPos = revisionPart.find("|", nStart);
                        if(nPos == std::string::npos)
                        {
                            return false;
                        }

                        revision.m_Date = revisionPart.substr(nStart, nPos - nStart - 1);

                        nPos = revisionPart.rfind(" line", revisionPart.length() - 1);
                        if(nPos == std::string::npos)
                        {
                            return false;
                        }

                        nStart = revisionPart.rfind("| ");
                        if(nStart == std::string::npos)
                        {
                            return false;
                        }

                        nCommentLines = atoi(revisionPart.substr(nStart + 2, nPos - nStart).c_str());
                    }
                    break;

            case 3:
                  revision.m_Description = revisionPart;
                  break;
            default:
                    if(nRevisionLine > 3 && nCommentLines > 0)
                    {
                        revision.m_Description += "\n" + revisionPart;
                    }
                    break;
            }

            nRevisionLine++;
            if(nRevisionLine > 3 && --nCommentLines <= 0)
            {
                nCommentLines = 1;
                nRevisionLine = 0;
                m_revisions.push_back(revision);
            }
        }

        return true;
    }

    const RevisionInfo::Collection& getRevisions() const
    {
        return m_revisions;
    }

private:
    RevisionInfo::Collection m_revisions;
    int m_nDisplayRevisionsCount;
};

class StatusSvnCommand : public SvnCommand
{
public:
    StatusSvnCommand(const std::string& path) : SvnCommand(path)
    {
    }

    virtual std::string getType() const { return "svn status"; }
    virtual bool execute()
    {
        std::string result = executeShellCommand(std::string("svn status ") + m_path);
        if(result.empty())
        {
            return false;
        }

        std::istringstream stringStream(result);
        std::string revisionPart;
        while(getline(stringStream, revisionPart, '\n'))
        {
            ChangeInfo info;
            info.m_Status = revisionPart.substr(0, 1);
            info.m_AffectedItem = revisionPart.substr(1);
            while(info.m_AffectedItem.size() && info.m_AffectedItem[0] == ' ')
            {
                info.m_AffectedItem = info.m_AffectedItem.substr(1);
            }

            m_changes.push_back(info);
        }
        return true;
    }

    const ChangeInfo::Collection& getChanges() const
    {
        return m_changes;
    }

private:
    ChangeInfo::Collection m_changes;
};

class DiffSvnCommand : public SvnCommand
{
public:
    DiffSvnCommand(const std::string& path, int nRevision)
        : SvnCommand(path)
        , m_nRevision(nRevision)
    {
    }

    virtual std::string getType() const { return "svn diff"; }
    virtual bool execute()
    {
        std::stringstream ss; ss << m_nRevision;
        std::string result = executeShellCommand(std::string("svn diff ") + m_path + " -c " + ss.str() + " --summarize");
        if(result.empty())
            return false;

        std::istringstream stringStream(result);
        std::string revisionPart;
        while(getline(stringStream, revisionPart, '\n'))
        {
            m_affectedItems.push_back(revisionPart);
        }
        return true;
    }

   std::list<std::string> getAffectedItems() const
   {
       return m_affectedItems;
   }

   int getRevision() const
   {
       return m_nRevision;
   }
private:
    int m_nRevision;
    std::list<std::string> m_affectedItems;
};

class LaunchDiffViewerSvnCommand : public SvnCommand
{
public:
    LaunchDiffViewerSvnCommand(const std::string& path, int nRevision)
        : SvnCommand(path)
        , m_nRevision(nRevision)
    {
    }

    virtual std::string getType() const { return "svn diff --diff-cmd"; }
    virtual bool execute()
    {
        //launch meld
        std::stringstream ss;
        ss << "svn diff --diff-cmd meld " << m_path;
        if(m_nRevision != -1)
        {
            ss << " -c " << m_nRevision;
        }
        ss << " &";
        system(ss.str().c_str());
        return true;
    }

private:
    int m_nRevision;
};

class ListSvnCommand : public SvnCommand
{
public:
    ListSvnCommand(const std::string& path, RepoItemInfo::SmartPtr repoInfo)
        : SvnCommand(path)
    {
        m_repoInfo = repoInfo;
    }

    virtual std::string getType() const { return "svn list"; }
    virtual bool execute()
    {
        std::string result = executeShellCommand(std::string("svn list ") + m_repoInfo->getFullPath());
        if(result.empty())
        {
            return false;
        }

        std::istringstream stringStream(result);
        std::string repoItem;
        m_repoInfo->m_subItems.clear();
        while(getline(stringStream, repoItem, '\n'))
        {
            if(repoItem.length())
            {
                RepoItemInfo::ItemType itemType = RepoItemInfo::File;
                if(repoItem[repoItem.length() - 1] == '/')
                {
                    itemType = RepoItemInfo::Directory;
                }

                m_repoInfo->m_subItems.push_back(RepoItemInfo::SmartPtr(new RepoItemInfo(m_repoInfo.get(), repoItem, itemType)));
            }
        }
        return true;
    }

    const RepoItemInfo::SmartPtr getRepoInfo()
    {
        return  m_repoInfo;
    }
private:
    RepoItemInfo::SmartPtr m_repoInfo;
};

class InfoSvnCommand : public SvnCommand
{
public:
    InfoSvnCommand(const std::string& path)
        : SvnCommand(path)
        , m_nCurrentRevision(-1)
    {
    }

    virtual std::string getType() const { return "svn info"; }
    virtual bool execute()
    {
        std::string result = executeShellCommand(std::string("svn info ") + m_path);
        if(result.empty())
            return false;

        size_t nPos = result.find("Last Changed Rev: ");
        if(nPos == std::string::npos)
        {
            return false;
        }

        size_t nEndPos = result.find("\n", nPos);
        if(nEndPos == std::string::npos)
        {
            return false;
        }

        m_nCurrentRevision = atoi(result.substr(nPos + 17, nEndPos - nPos - 17).c_str());


        nPos = result.find("URL: ");
        if(nPos == std::string::npos)
        {
            return false;
        }

        nEndPos = result.find("\n", nPos);
        if(nEndPos == std::string::npos)
        {
            return false;
        }

        m_repoURL = result.substr(nPos + 5, nEndPos - nPos - 5);

        return true;
    }

    int getCurrentRevision() const
    {
        return m_nCurrentRevision;
    }

    std::string getRepoUrl() const
    {
        return m_repoURL;
    }

private:
    int m_nCurrentRevision;
    std::string m_repoURL;
};

class UpdateSvnCommand : public SvnCommand
{
public:
    UpdateSvnCommand(const std::string& path, int nRevision = -1 /*convention -1 = HEAD*/)
        : SvnCommand(path)
        , m_nRevision(nRevision)
    {
    }

    virtual std::string getType() const { return "svn update"; }
    virtual bool execute()
    {
        std::stringstream ss;
        if(m_nRevision != -1)
        {
            ss << " -r " << m_nRevision;
        }
        std::string result = executeShellCommand(std::string("svn update ") + m_path + ss.str() + " --non-interactive");
        if(result.empty())
            return false;

        return true;
    }

private:
    int m_nRevision;
};

class AddSvnCommand : public SvnCommand
{
public:
    AddSvnCommand(const std::string& path, const std::string& addItem)
        : SvnCommand(path)
        , m_addItem(addItem)
    {
    }

    virtual std::string getType() const { return "svn add"; }
    virtual bool execute()
    {
        std::string result = executeShellCommand(std::string("svn add ") + m_addItem + " --non-interactive");
        if(result.empty())
            return false;

        return true;
    }

private:
    std::string m_addItem;
};

class RevertSvnCommand : public SvnCommand
{
public:
    RevertSvnCommand(const std::string& path, const std::string& revertItem)
        : SvnCommand(path)
        , m_revertItem(revertItem)
    {
    }

    virtual std::string getType() const { return "svn revert"; }
    virtual bool execute()
    {
        std::string result = executeShellCommand(std::string("svn revert ") + m_revertItem + " --non-interactive");
        if(result.empty())
            return false;

        return true;
    }

private:
    std::string m_revertItem;
};

class CommitSvnCommand : public SvnCommand
{
public:
    CommitSvnCommand(const std::string& path, const std::list<std::string>& commitItems, const std::string& message)
        : SvnCommand(path)
        , m_commitItems(commitItems)
        , m_message(message)
    {
    }

    virtual std::string getType() const { return "svn commit"; }
    virtual bool execute()
    {
        if(m_commitItems.empty() || m_message.empty() || m_path.empty())
            return false;

        std::string command = "svn commit";
        for(auto item : m_commitItems)
        {
            command += " " + item;
        }

        //create the commit message file
        std::string strCommitFileName = AppSettings::instance()->getTempPath() + "commitMessageFile.txt";
        executeShellCommand(std::string(">") + strCommitFileName);

        std::istringstream stringStream(m_message);
        std::string messageLine;
        while(getline(stringStream, messageLine, '\n'))
        {
            executeShellCommand(std::string("echo \"") + messageLine + "\">>" + strCommitFileName);
        }

        //commit with message from file
        command +=" -F \"" + strCommitFileName + "\"";
        std::string result;
        result = executeShellCommand(command + " --non-interactive");
        if(result.empty())
        {
            return false;
        }

        return true;
    }

private:
    const std::list<std::string> m_commitItems;
    const std::string m_message;
};

#endif // SVNCOMMANDS_H
