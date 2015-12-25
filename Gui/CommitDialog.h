#ifndef COMMITDIALOG_H
#define COMMITDIALOG_H

#include <QDialog>
#include <qstandarditemmodel.h>

#include "Repos/SVN/SvnViewer.h"
#include "Gui/CommonUI.h"

namespace Ui {
class CommitDialog;
}

class CommitDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CommitDialog(RepoDialogsObserver* pObserver, QWidget *parent = 0);
    ~CommitDialog();

    void updateChanges(const ChangeInfo::Collection& localChanges);
    const std::list<std::string>& getSelectedForCommitItems() const {return m_selectedForCommitItems;}
    std::string getMessage() const { return m_commitMessage; }
private slots:
    void on_okButton_clicked();

    void on_tableWidget_doubleClicked(const QModelIndex &index);
    void on_cancelButton_clicked();
    void on_tableWidget_customContextMenuRequested(const QPoint &pos);
    void on_revert();
    void on_add_to_source_control();
    void on_show_changes();

    void on_showUnversioned_toggled(bool checked);

private:
    QString getSelectedCommitItemStatus(const QModelIndex &index) const;
    QString getSelectedCommitItemName(const QModelIndex &index) const;
    void updatedSelectedForCommitItems();
private:
    Ui::CommitDialog *ui;
    std::list<std::string> m_selectedForCommitItems;
    ChangeInfo::Collection m_localChanges;
    std::string m_commitMessage;
    RepoDialogsObserver* m_pObserver;
};

#endif // COMMITDIALOG_H
