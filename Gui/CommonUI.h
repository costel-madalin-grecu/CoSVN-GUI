#ifndef COMMONUI_H
#define COMMONUI_H

#include <QString>

class RepoDialogsObserver
{
public:
    virtual void onLaunchDiff(const QString& strItem) = 0;
    virtual void onRevertModifiedItem(const QString& strItem) = 0;
    virtual void onAdToSourceControl(const QString& strItem) = 0;
};

#endif // COMMONUI_H
