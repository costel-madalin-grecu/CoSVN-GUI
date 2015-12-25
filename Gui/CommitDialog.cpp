#include "CommitDialog.h"
#include "ui_CommitDialog.h"
#include <QCheckBox>
#include <QLabel>
#include <QMessageBox>
#include <QMenu>


CommitDialog::CommitDialog(RepoDialogsObserver* pObserver, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CommitDialog),
    m_pObserver(pObserver)
{
    ui->setupUi(this);

    ui->tableWidget->setColumnCount(2);;

    QStringList lineItems;
    lineItems << "Status" << "Changed items";
    ui->tableWidget->setHorizontalHeaderLabels(lineItems);
    ui->tableWidget->horizontalHeader()->setVisible(true);
    ui->tableWidget->verticalHeader()->setVisible(false);
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidget->setShowGrid(false);

    on_showUnversioned_toggled(false);
}

CommitDialog::~CommitDialog()
{
    delete ui;
}

void CommitDialog::updateChanges(const ChangeInfo::Collection& localChanges)
{
    updatedSelectedForCommitItems();

    m_localChanges = localChanges;
    on_showUnversioned_toggled(ui->showUnversioned->isChecked());
}

void CommitDialog::on_okButton_clicked()
{
    updatedSelectedForCommitItems();

    QString escapedMessage = ui->textEdit->toPlainText();
    m_commitMessage = escapedMessage.toStdString();
    QDialog::accept();
}

QString CommitDialog::getSelectedCommitItemStatus(const QModelIndex &index) const
{
    QTableWidgetItem* pSelectedItemStatus = ui->tableWidget->item(index.row(), 0);
    if(pSelectedItemStatus )
    {
        return pSelectedItemStatus->text();
    }

    return QString();
}

QString CommitDialog::getSelectedCommitItemName(const QModelIndex &index) const
{
    QTableWidgetItem* pSelectedChanged1Item = ui->tableWidget->item(index.row(), 1);
    if(pSelectedChanged1Item)
    {
        return pSelectedChanged1Item->text();
    }

    return QString();
}

void CommitDialog::on_tableWidget_doubleClicked(const QModelIndex &index)
{
    if(!getSelectedCommitItemStatus(index).startsWith("?"))
    {
        QString affectedItem = getSelectedCommitItemName(index);
        if(!affectedItem.isEmpty())
        {
            m_pObserver->onLaunchDiff(affectedItem);
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

void CommitDialog::on_cancelButton_clicked()
{
    QDialog::reject();
}

void CommitDialog::on_revert()
{
    QList<QTableWidgetItem*> selectedItems = ui->tableWidget->selectedItems();
    if(!selectedItems.size())
    {
        return;
    }

    const QModelIndex& index = ui->tableWidget->model()->index(selectedItems.at(0)->row(), selectedItems.at(0)->column());
    QString affectedItem = getSelectedCommitItemName(index);
    m_pObserver->onRevertModifiedItem(affectedItem);
}

void CommitDialog::on_add_to_source_control()
{
    QList<QTableWidgetItem*> selectedItems = ui->tableWidget->selectedItems();
    if(!selectedItems.size())
    {
        return;
    }

    const QModelIndex& index = ui->tableWidget->model()->index(selectedItems.at(0)->row(), selectedItems.at(0)->column());
    QString affectedItem = getSelectedCommitItemName(index);
    m_pObserver->onAdToSourceControl(affectedItem);
}

void CommitDialog::on_show_changes()
{
    QList<QTableWidgetItem*> selectedItems = ui->tableWidget->selectedItems();
    if(!selectedItems.size())
    {
        return;
    }

    const QModelIndex& index = ui->tableWidget->model()->index(selectedItems.at(0)->row(), selectedItems.at(0)->column());
    QString affectedItem = getSelectedCommitItemName(index);
    m_pObserver->onLaunchDiff(affectedItem);
}

void CommitDialog::on_tableWidget_customContextMenuRequested(const QPoint &pos)
{
    updatedSelectedForCommitItems();

    QList<QTableWidgetItem*> selectedItems = ui->tableWidget->selectedItems();
    if(!selectedItems.size())
    {
        return;
    }

    const QModelIndex& index = ui->tableWidget->model()->index(selectedItems.at(0)->row(), selectedItems.at(0)->column());
    if(getSelectedCommitItemStatus(index).startsWith("?"))
    {
        QMenu* contextMenu = new QMenu(ui->tableWidget);
        contextMenu->addAction("Add to source control", this, SLOT(on_add_to_source_control()));
        contextMenu->popup(ui->tableWidget->viewport()->mapToGlobal(pos));
    }
    else
    {
        QMenu* contextMenu = new QMenu(ui->tableWidget);
        contextMenu->addAction("Revert", this, SLOT(on_revert()));
        contextMenu->addAction("Show changes", this, SLOT(on_show_changes()));
        contextMenu->popup(ui->tableWidget->viewport()->mapToGlobal(pos));
    }
}

void CommitDialog::on_showUnversioned_toggled(bool checked)
{
    ui->tableWidget->setVisible(false);

    while(ui->tableWidget->rowCount())
    {
        ui->tableWidget->removeRow(0);
    }

    for(ChangeInfo::Collection::const_iterator it = m_localChanges.begin(); it != m_localChanges.end(); ++it)
    {
        if(!checked && QString(it->m_Status.c_str()).startsWith("?"))
        {
            continue;
        }

        int nItem = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(nItem);

        QTableWidgetItem* item = new QTableWidgetItem(QString(it->m_Status.c_str()));
        if(!item->text().startsWith("?"))
        {
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(Qt::Unchecked);

            for(auto selectedItem : m_selectedForCommitItems)
            {
                if(selectedItem == it->m_AffectedItem)
                {
                    item->setCheckState(Qt::Checked);
                    break;
                }
            }
        }
        ui->tableWidget->setItem(nItem, 0, item);

        item = new QTableWidgetItem(QString(it->m_AffectedItem.c_str()));
        ui->tableWidget->setItem(nItem, 1, item);
    }


    ui->tableWidget->setVisible(true);
}

void CommitDialog::updatedSelectedForCommitItems()
{
    m_selectedForCommitItems.clear();
    for(int i = 0; i < ui->tableWidget->rowCount(); i++)
    {
        QTableWidgetItem* pSelectedItemStatus = ui->tableWidget->item(i, 0);
        if(pSelectedItemStatus && pSelectedItemStatus->checkState() == Qt::Checked)
        {
            QTableWidgetItem* pCommitItem = ui->tableWidget->item(i, 1);
            if(pCommitItem)
            {
                m_selectedForCommitItems.push_back(pCommitItem->text().toStdString());
            }
        }
    }
}
