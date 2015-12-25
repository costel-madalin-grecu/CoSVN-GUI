#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "Gui/ChooseRepoDialog.h"
#include "Gui/CommitDialog.h"
#include "Gui/StatusDialog.h"
#include "Gui/CommonUI.h"
#include "Repos/SVN/SvnViewer.h"


#include <QMainWindow>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QItemSelection>
#include <QApplication>
#include <QTreeWidgetItem>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow, public SvnViewerObserver, public RepoDialogsObserver
{
    Q_OBJECT

public:
    explicit MainWindow(QApplication& app, QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_actionOpen_triggered();
    void on_revisionsTable_clicked(const QModelIndex &index);
    void on_actionRefresh_triggered();
    void on_actionExit_triggered();
    void on_revisionDetails_doubleClicked(const QModelIndex &index);
    void on_actionUpdate_to_head_triggered();
    void on_actionCheck_for_modifications_triggered();
    void on_actionCommit_triggered();
    void on_revisionsTable_customContextMenuRequested(const QPoint &pos);
    void on_update_to_revision();
    void on_revisionsTable_selection_changed(const QItemSelection & selected, const QItemSelection & deselected);
    void on_actionAbout_triggered();
    void on_treeWidgetRepo_itemExpanded(QTreeWidgetItem *item);
    void on_treeWidgetRepo_itemDoubleClicked(QTreeWidgetItem *item, int column);
    void on_revisionsFilterEdit_textChanged(const QString &arg1);
    void on_treeWidgetRepo_customContextMenuRequested(const QPoint &pos);
    void on_show_logs_at_node();

private:

    //SvnViewerObserver
    virtual void onRevisionsListUpdated();
    virtual void onAffectedItemsUpdated();
    virtual void onLocalModificationsUpdated();
    virtual void onErrosGenerated();
    virtual void onRepoContentUpdated();

    //RepoDialogsObserver
    virtual void onLaunchDiff(const QString& strItem);
    virtual void onRevertModifiedItem(const QString& strItem);
    virtual void onAdToSourceControl(const QString& strItem);


    int getSelectedRevision() const;
    void performInitialUpdates(QObject* filter);
    static QString getPathToRoot(const QTreeWidgetItem* pTreeItem);

    static void fillParentItem(RepoItemInfo::SmartPtr& repoItem, QTreeWidgetItem* parentItem, QTreeWidget* parentView);
    static void fillChildItems(RepoItemInfo::SmartPtr& repoItem, QTreeWidgetItem* parentItem);
    static void updateTreeItemState(QTreeWidgetItem* pTreeItem, ChangeInfo::Collection& localChanges);

    void displayRevisionsList();
    void displayLocalChanges();
    void displayAffectedItems();
    void displayRepoContent();
private:
    Ui::MainWindow *ui;
    QStandardItemModel *modelRevisions;
    QStandardItemModel *modelAffectedItems;

    friend class RefreshGuiEventFilter;
    QApplication& m_app;
    bool m_bInitalUpdatePerfromed;

    StatusDialog* m_activeStatusDialog;
    CommitDialog* m_activeCommitDialog;
    QString m_currentRepoPath;
};

#endif // MAINWINDOW_H
