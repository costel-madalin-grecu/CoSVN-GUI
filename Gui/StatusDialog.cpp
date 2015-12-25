#include "StatusDialog.h"
#include "ui_StatusDialog.h"
#include <qmessagebox.h>
#include <QMenu>

StatusDialog::StatusDialog(RepoDialogsObserver* pObserver, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::StatusDialog),
    m_pObserver(pObserver)
{
    ui->setupUi(this);

    modelChanges = new QStandardItemModel(this);

    QStringList lineItems;
    lineItems << "Status" << "Changed items";

    modelChanges->setColumnCount(2);
    modelChanges->setHorizontalHeaderLabels(lineItems);


    ui->tableView->setModel(modelChanges);
    ui->tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableView->horizontalHeader()->setStretchLastSection(true);
    ui->tableView->setShowGrid(false);
    ui->tableView->verticalHeader()->setVisible(false);

    showChanges(false);
}

StatusDialog::~StatusDialog()
{
    delete ui;
}

void StatusDialog::updateChanges(const ChangeInfo::Collection& localChanges)
{
    m_localChanges = localChanges;
    showChanges(ui->checkBox->isChecked());
}

void StatusDialog::on_closeButton_clicked()
{
    QDialog::accept();
}

void StatusDialog::on_tableView_doubleClicked(const QModelIndex &index)
{
    QModelIndex indexStatus = modelChanges->index(index.row(), 0);
    QStandardItem* pSelectedItemStatus = modelChanges->itemFromIndex(indexStatus);
    if(pSelectedItemStatus )
    {
        if(!pSelectedItemStatus->text().startsWith("?"))
        {
            QModelIndex indexChangedItem = modelChanges->index(index.row(), 1);
            QStandardItem* pSelectedChanged1Item = modelChanges->itemFromIndex(indexChangedItem);
            if(pSelectedChanged1Item)
            {   
                m_pObserver->onLaunchDiff(pSelectedChanged1Item->text());
            }
        }
        else
        {
            QMessageBox messageBox;
            messageBox.critical(0,"Error", "Cannot show changes for this item.");
            messageBox.setFixedSize(500,200);

            messageBox.show();
        }
    }
}

void StatusDialog::on_checkBox_clicked(bool checked)
{
    showChanges(checked);
}

void StatusDialog::showChanges(bool bShowUnversioned)
{
    ui->tableView->setVisible(false);

    while (modelChanges->rowCount() > 0)
    {
        modelChanges->removeRow(0);
    }

    for(ChangeInfo::Collection::const_iterator it = m_localChanges.begin(); it != m_localChanges.end(); ++it)
    {
        if(!bShowUnversioned && QString(it->m_Status.c_str()).startsWith("?"))
        {
            continue;
        }

        QList<QStandardItem*> lineItems;

        QStandardItem* pItem = new QStandardItem(it->m_Status.c_str());
        lineItems.append(pItem);

        pItem = new QStandardItem(it->m_AffectedItem.c_str());
        lineItems.append(pItem);


        modelChanges->appendRow(lineItems);
    }


    ui->tableView->resizeColumnsToContents();
    ui->tableView->setVisible(true);
}

QString StatusDialog::getSelectedItemStatus() const
{
    QModelIndexList selectedItems = ui->tableView->selectionModel()->selectedIndexes();
    if(!selectedItems.size())
    {
        return QString();
    }

    for(QModelIndexList::iterator it = selectedItems.begin(); it != selectedItems.end(); ++it)
    {
        QStandardItem* pSelectedItem = modelChanges->itemFromIndex(modelChanges->index(it->row(), 0));
        return pSelectedItem->text();
    }

    return QString();
}

QString StatusDialog::getSelectedItemName() const
{
    QModelIndexList selectedItems = ui->tableView->selectionModel()->selectedIndexes();
    if(!selectedItems.size())
    {
        return QString();
    }

    for(QModelIndexList::iterator it = selectedItems.begin(); it != selectedItems.end(); ++it)
    {
        QStandardItem* pSelectedItem = modelChanges->itemFromIndex(modelChanges->index(it->row(), 1));
        return pSelectedItem->text();
    }

    return QString();
}

void StatusDialog::on_tableView_customContextMenuRequested(const QPoint &pos)
{
    QString strStatus = getSelectedItemStatus();
    if(strStatus.isEmpty())
    {
        return;
    }

    if(strStatus.startsWith("?"))
    {
        QMenu* contextMenu = new QMenu(ui->tableView);
        contextMenu->addAction("Add to source control", this, SLOT(on_add_to_source_control()));
        contextMenu->popup(ui->tableView->viewport()->mapToGlobal(pos));
    }
    else
    {
        QMenu* contextMenu = new QMenu(ui->tableView);
        contextMenu->addAction("Revert", this, SLOT(on_revert()));
        contextMenu->addAction("Show changes", this, SLOT(on_show_changes()));
        contextMenu->popup(ui->tableView->viewport()->mapToGlobal(pos));
    }
}

void StatusDialog::on_add_to_source_control()
{
    QString affectedItem = getSelectedItemName();
    if(affectedItem.isEmpty())
    {
        return;
    }
    m_pObserver->onAdToSourceControl(affectedItem);
}

void StatusDialog::on_show_changes()
{
    QString affectedItem = getSelectedItemName();
    if(affectedItem.isEmpty())
    {
        return;
    }
    m_pObserver->onLaunchDiff(affectedItem);
}

void StatusDialog::on_revert()
{
    QString affectedItem = getSelectedItemName();
    if(affectedItem.isEmpty())
    {
        return;
    }
    m_pObserver->onRevertModifiedItem(affectedItem);
}
