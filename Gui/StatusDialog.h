#ifndef STATUSDIALOG_H
#define STATUSDIALOG_H

#include <QDialog>
#include <qstandarditemmodel.h>

#include "Repos/SVN/SvnViewer.h"
#include "Gui/CommonUI.h"

namespace Ui {
class StatusDialog;
}

class StatusDialog : public QDialog
{
    Q_OBJECT

public:
    explicit StatusDialog(RepoDialogsObserver* pObserver, QWidget *parent = 0);
    ~StatusDialog();

    void updateChanges(const ChangeInfo::Collection& localChanges);

private:
    void showChanges(bool bShowUnversioned);
    QString getSelectedItemStatus() const;
    QString getSelectedItemName() const;

private slots:
    void on_closeButton_clicked();
    void on_tableView_doubleClicked(const QModelIndex &index);
    void on_checkBox_clicked(bool checked);
    void on_tableView_customContextMenuRequested(const QPoint &pos);
    void on_add_to_source_control();
    void on_show_changes();
    void on_revert();

private:
    Ui::StatusDialog *ui;
    QStandardItemModel *modelChanges;
    ChangeInfo::Collection m_localChanges;
    RepoDialogsObserver* m_pObserver;
};

#endif // STATUSDIALOG_H
