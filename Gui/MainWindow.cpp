#include "MainWindow.h"
#include "ui_MainWindow.h"

#include "Settings/AppSettings.h"
#include "Gui/AboutDialog.h"

#include <QTreeWidgetItemIterator>

#include <QStyledItemDelegate>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPainter>

#include <qtimer.h>

class HtmlDelegate : public QStyledItemDelegate
{
protected:
    void paint ( QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const;
    QSize sizeHint ( const QStyleOptionViewItem & option, const QModelIndex & index ) const;
};

void HtmlDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItemV4 optionV4 = option;
    initStyleOption(&optionV4, index);

    QStyle *style = optionV4.widget? optionV4.widget->style() : QApplication::style();

    QTextDocument doc;
    doc.setHtml(optionV4.text);

    /// Painting item without text
    optionV4.text = QString();
    style->drawControl(QStyle::CE_ItemViewItem, &optionV4, painter);

    QAbstractTextDocumentLayout::PaintContext ctx;

    // Highlighting text if item is selected
    if (optionV4.state & QStyle::State_Selected)
        ctx.palette.setColor(QPalette::Text, optionV4.palette.color(QPalette::Active, QPalette::HighlightedText));

    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &optionV4);
    painter->save();
    painter->translate(textRect.topLeft());
    painter->setClipRect(textRect.translated(-textRect.topLeft()));
    doc.documentLayout()->draw(painter, ctx);
    painter->restore();
}

QSize HtmlDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItemV4 optionV4 = option;
    initStyleOption(&optionV4, index);

    QTextDocument doc;
    doc.setHtml(optionV4.text);
    doc.setTextWidth(optionV4.rect.width());
    return QSize(doc.idealWidth(), doc.size().height());
}

static const QEvent::Type REVISIONS_UPDATED = (QEvent::Type)QEvent::registerEventType();
static const QEvent::Type LOCAL_CHANGES_UPDATED = (QEvent::Type)QEvent::registerEventType();
static const QEvent::Type AFFECTED_ITEMS_UPDATED = (QEvent::Type)QEvent::registerEventType();
static const QEvent::Type REPO_CONTENT_UPDATED = (QEvent::Type)QEvent::registerEventType();


class RefreshGuiEventFilter : public QObject
{
public:
    RefreshGuiEventFilter(MainWindow* pMainWindow)
    {
        m_pMainWindow = pMainWindow;
    }

protected:
    bool eventFilter(QObject* /*obj*/, QEvent* event)
    {
        if(event)
        {
            if(event->type() == QEvent::Show)
            {
                m_pMainWindow->performInitialUpdates(this);
            }

            if(event->type() == REVISIONS_UPDATED)
            {
                m_pMainWindow->displayRevisionsList();
                return true;
            }

            if(event->type() == LOCAL_CHANGES_UPDATED)
            {
                m_pMainWindow->displayLocalChanges();
                return true;
            }

            if(event->type() == AFFECTED_ITEMS_UPDATED)
            {
                m_pMainWindow->displayAffectedItems();
                return true;
            }

            if(event->type() == REPO_CONTENT_UPDATED)
            {
                m_pMainWindow->displayRepoContent();
                return true;
            }
        }

        return false;
    }

    MainWindow* m_pMainWindow;
};

MainWindow::MainWindow(QApplication &app, QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_app(app),
    m_activeStatusDialog(nullptr),
    m_activeCommitDialog(nullptr)
{
    ui->setupUi(this);

    modelRevisions = new QStandardItemModel(this);

    QStringList lineItems;
    lineItems << "Revision" << "Date" << "Author" << "Description";

    modelRevisions->setColumnCount(4);
    modelRevisions->setHorizontalHeaderLabels(lineItems);


    ui->revisionsTable->setModel(modelRevisions);
    ui->revisionsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->revisionsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->revisionsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->revisionsTable->horizontalHeader()->setStretchLastSection(true);
    ui->revisionsTable->setShowGrid(false);
    ui->revisionsTable->verticalHeader()->setVisible(false);

    modelAffectedItems = new QStandardItemModel(this);

    lineItems.clear();
    lineItems << "Affected items";

    modelAffectedItems->setColumnCount(1);
    modelAffectedItems->setHorizontalHeaderLabels(lineItems);


    ui->revisionDetails->setModel(modelAffectedItems);
    ui->revisionDetails->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->revisionDetails->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->revisionDetails->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->revisionDetails->horizontalHeader()->setStretchLastSection(true);
    ui->revisionDetails->setShowGrid(false);
    ui->revisionDetails->verticalHeader()->setVisible(false);

    RefreshGuiEventFilter* pWatcher = new RefreshGuiEventFilter(this);
    this->installEventFilter(pWatcher);

    ui->lebelRevision->setText("");
    ui->lebelDate->setText("");
    ui->lebelAuthor->setText("");
    ui->lebelDescription->setText("");

    ui->treeWidgetRepo->header()->setVisible(false);

    connect(ui->revisionsTable->selectionModel(),
            SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)),
            SLOT(on_revisionsTable_selection_changed(QItemSelection,QItemSelection)));

    m_bInitalUpdatePerfromed = false;

    SvnViewer::instance()->setObserver(this);

    //set auto refresh interval at 2 min
    QTimer* timer = new QTimer(this);
    timer->setInterval(120000);
    connect(timer, SIGNAL(timeout()), SLOT(on_actionRefresh_triggered()));
    timer->start();

    //ui->revisionsTable->setItemDelegate(new HtmlDelegate());
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionOpen_triggered()
{
    ChooseRepoDialog dlg(this, AppSettings::instance()->getRecents());
    int nResult = dlg.exec();
    if(nResult == QDialog::Accepted)
    {
        ui->treeWidgetRepo->clear();
        if(modelAffectedItems->rowCount())
        {
            modelAffectedItems->removeRows(0, modelAffectedItems->rowCount());
        }

        if(modelRevisions->rowCount())
        {
            modelRevisions->removeRows(0, modelRevisions->rowCount());
        }

        m_currentRepoPath = dlg.getSelectedPath().c_str();
        SvnViewer::instance()->init(dlg.getSelectedPath());
    }
}

void MainWindow::on_actionRefresh_triggered()
{
    if(SvnViewer::instance()->isInitialized())
    {
        m_currentRepoPath = SvnViewer::instance()->getRepoPath().c_str();
        SvnViewer::instance()->refresh();
    }
}

void MainWindow::on_revisionsTable_clicked(const QModelIndex& /*index*/)
{
    displayAffectedItems();
}


void MainWindow::on_actionExit_triggered()
{
    this->close();
}

void MainWindow::on_revisionDetails_doubleClicked(const QModelIndex &index)
{
    int nRevision = -1;
    QItemSelectionModel *selectionModel = ui->revisionsTable->selectionModel();
    if(selectionModel && selectionModel->hasSelection())
    {
        QModelIndexList selectedList = selectionModel->selectedIndexes();
        for(QModelIndexList::iterator it = selectedList.begin(); it != selectedList.end(); ++it)
        {
            QStandardItem* pSelectedItem = modelRevisions->itemFromIndex(modelRevisions->index(it->row(), 0));
            nRevision = pSelectedItem->text().toInt();
            break;
        }
    }

    if(nRevision == -1)
    {
        return;
    }

    QStandardItem* pSelectedItem = modelAffectedItems->itemFromIndex(index);
    if(pSelectedItem)
    {
        //remove status (M, A, ...)
        QString strPath = pSelectedItem->text().mid(1);

        //trim spaces
        while(strPath[0] == ' ')
        {
            strPath = strPath.mid(1);
        }

        SvnViewer::instance()->launchDiffViewer(strPath.toStdString(), nRevision);
    }
}

void MainWindow::on_actionUpdate_to_head_triggered()
{
    if(!SvnViewer::instance()->isInitialized())
        return;

    SvnViewer::instance()->updateToHead();
}

void MainWindow::on_actionCheck_for_modifications_triggered()
{
    if(!SvnViewer::instance()->isInitialized())
        return;

    SvnViewer::instance()->checkForModifications();

    StatusDialog dlg(this);
    m_activeStatusDialog = &dlg;
    m_activeStatusDialog->updateChanges(SvnViewer::instance()->getLocalChanges());
    dlg.exec();
    m_activeStatusDialog = nullptr;
}

void MainWindow::on_actionCommit_triggered()
{
    if(!SvnViewer::instance()->isInitialized())
        return;

    CommitDialog dlg(this, this);
    m_activeCommitDialog = &dlg;
    m_activeCommitDialog ->updateChanges(SvnViewer::instance()->getLocalChanges());
    if(dlg.exec() == QDialog::Accepted)
    {
        SvnViewer::instance()->commit(dlg.getSelectedForCommitItems(), dlg.getMessage())    ;
    }
    m_activeCommitDialog  = nullptr;
}

void MainWindow::on_revisionsTable_customContextMenuRequested(const QPoint &pos)
{
    int nRevision = getSelectedRevision();
    if(nRevision == -1)
    {
        return;
    }

    QMenu* contextMenu = new QMenu(ui->revisionsTable);
    contextMenu->addAction("Update to this revision", this, SLOT(on_update_to_revision()));
    contextMenu->popup(ui->revisionsTable->viewport()->mapToGlobal(pos));
}


void MainWindow::on_update_to_revision()
{
    int nRevision = getSelectedRevision();
    if(nRevision == -1)
    {
        return;
    }

    SvnViewer::instance()->updateToRevision(nRevision);
}

void MainWindow::onLaunchDiff(const QString& strItem)
{
    if(!SvnViewer::instance()->isInitialized())
    {
        return;
    }

    SvnViewer::instance()->launchDiffViewer(strItem.toStdString(), -1);
}

void MainWindow::onRevertModifiedItem(const QString& strItem)
{
    if(!SvnViewer::instance()->isInitialized())
        return;

    SvnViewer::instance()->revert(strItem.toStdString());
}

void MainWindow::onAdToSourceControl(const QString& strItem)
{
    if(!SvnViewer::instance()->isInitialized())
        return;

    SvnViewer::instance()->addToSourceControl(strItem.toStdString());
}

void MainWindow::on_revisionsTable_selection_changed(const QItemSelection& selected, const QItemSelection& /*deselected*/)
{
    if(!selected.empty() && !selected.indexes().empty())
    {
        on_revisionsTable_clicked(selected.indexes().first());
    }
}

void MainWindow::on_actionAbout_triggered()
{
    AboutDialog dialog(this);
    dialog.exec();
}

void MainWindow::on_treeWidgetRepo_itemExpanded(QTreeWidgetItem *item)
{
    if(SvnViewer::instance()->isInitialized() && item->childIndicatorPolicy() == QTreeWidgetItem::ShowIndicator)
    {
        if(item->childCount())
        {
            return;
        }
        QString pathToRot = getPathToRoot(item);
        SvnViewer::instance()->listContent(pathToRot.toStdString());
    }
}

void MainWindow::on_treeWidgetRepo_itemDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    if(SvnViewer::instance()->isInitialized() && item->childIndicatorPolicy() != QTreeWidgetItem::ShowIndicator)
    {
        ChangeInfo::Collection localChanges = SvnViewer::instance()->getLocalChanges();
        QString pathToRoot = getPathToRoot(item);
        ChangeInfo::Collection::const_iterator itChanges = localChanges.begin();
        for(; itChanges != localChanges.end(); ++itChanges)
        {
            if(QString(itChanges->m_AffectedItem.c_str()).contains(pathToRoot, Qt::CaseSensitive))
            {
                break;
            }
        }

        if(itChanges != localChanges.end())
        {
            SvnViewer::instance()->launchDiffViewer(pathToRoot.toStdString(), -1);
        }
        else
        {
            QString command ="${EDITOR:-gedit} " + pathToRoot;
            system(command.toStdString().c_str());
        }
    }
}

void MainWindow::on_revisionsFilterEdit_textChanged(const QString& /*arg1*/)
{
    displayRevisionsList();
}

void MainWindow::on_treeWidgetRepo_customContextMenuRequested(const QPoint &pos)
{
    QMenu* contextMenu = new QMenu(ui->treeWidgetRepo);
    contextMenu->addAction("Show logs", this, SLOT(on_show_logs_at_node()));
    contextMenu->popup(ui->treeWidgetRepo->viewport()->mapToGlobal(pos));
}

void MainWindow::on_show_logs_at_node()
{
    if(ui->treeWidgetRepo->selectedItems().size() != 1)
    {
        return;
    }

    QTreeWidgetItem* pSelectedItem = ui->treeWidgetRepo->selectedItems().back();
    QString strFullRepoPath = getPathToRoot(pSelectedItem);

    if(modelAffectedItems->rowCount())
    {
        modelAffectedItems->removeRows(0, modelAffectedItems->rowCount());
    }

    if(modelRevisions->rowCount())
    {
        modelRevisions->removeRows(0, modelRevisions->rowCount());
    }

    m_currentRepoPath = strFullRepoPath;
    SvnViewer::instance()->viewLog(strFullRepoPath.toStdString());
}

void MainWindow::onRevisionsListUpdated()
{
    m_app.postEvent(this, new QEvent(REVISIONS_UPDATED));
}

void MainWindow::onLocalModificationsUpdated()
{
    m_app.postEvent(this, new QEvent(LOCAL_CHANGES_UPDATED));
}

void MainWindow::onAffectedItemsUpdated()
{
    m_app.postEvent(this, new QEvent(AFFECTED_ITEMS_UPDATED));
}

void MainWindow::onErrosGenerated()
{

}

void MainWindow::onRepoContentUpdated()
{
    m_app.postEvent(this, new QEvent(REPO_CONTENT_UPDATED));
}

int MainWindow::getSelectedRevision() const
{
    int nRevision = -1;
    QItemSelectionModel *selectionModel = ui->revisionsTable->selectionModel();
    if(selectionModel && selectionModel->hasSelection())
    {
        QModelIndexList selectedList = selectionModel->selectedIndexes();
        for(QModelIndexList::iterator it = selectedList.begin(); it != selectedList.end(); ++it)
        {
            QStandardItem* pSelectedItem = modelRevisions->itemFromIndex(modelRevisions->index(it->row(), 0));
            nRevision = pSelectedItem->text().toInt();
            break;
        }
    }

    return nRevision;
}

void MainWindow::performInitialUpdates(QObject* /*filter*/)
{
    if(m_bInitalUpdatePerfromed)
    {
        return;
    }

    QList<int> newSizes;
    newSizes.push_back(ui->centralWidget->height()-10);
    newSizes.push_back(10);
    ui->splitterHorizontal->setSizes(newSizes);

    newSizes.clear();
    newSizes.push_back(ui->centralWidget->width()/4);
    newSizes.push_back(3*ui->centralWidget->width()/4);
    ui->splitterVerticalLowerPane->setSizes(newSizes);

    ui->splitterVerticalUpperPane->setSizes(newSizes);

    m_bInitalUpdatePerfromed = true;
}

QString MainWindow::getPathToRoot(const QTreeWidgetItem* pTreeItem)
{
    QString pathToRot = pTreeItem->text(0);
    while (pTreeItem->parent())
    {
        QString prefix = pTreeItem->parent()->text(0).endsWith("/") ? "" : "/";
        pathToRot = pTreeItem->parent()->text(0) + prefix + pathToRot;
        pTreeItem = pTreeItem->parent();
    }

    return pathToRot;
}

void MainWindow::displayRevisionsList()
{
    QString strFilter = ui->revisionsFilterEdit->text();

    RevisionInfo::Collection revisions = SvnViewer::instance()->getRevisionsList();
    int nCurrentRevision = SvnViewer::instance()->getCurrentRevision();

    //save selection
    QString strSelectedRevision = ui->lebelRevision->text();

    if(modelRevisions->rowCount())
    {
        modelRevisions->removeRows(0, modelRevisions->rowCount());
    }

    for(RevisionInfo::Collection::const_iterator it = revisions.begin(); it != revisions.end(); ++it)
    {
        bool bCurrentRev = (it->m_No == nCurrentRevision);
        bool bNewRev = (it->m_No > nCurrentRevision);

        QList<QStandardItem*> lineItems;
        std::stringstream ss; ss << it->m_No;
        QStandardItem* pItem = new QStandardItem(ss.str().c_str());

        if(bCurrentRev || bNewRev)
        {
            QFont font = pItem->font();
            if(bCurrentRev)
                font.setBold(true);
            else
                font.setItalic(true);
            pItem->setFont(font);
        }
        pItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

        lineItems.append(pItem);

        pItem = new QStandardItem(it->m_Date.c_str());
        if(bCurrentRev || bNewRev)
        {
            QFont font = pItem->font();
            if(bCurrentRev)
                font.setBold(true);
            else
                font.setItalic(true);
            pItem->setFont(font);
        }
        pItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        lineItems.append(pItem);

        pItem = new QStandardItem(it->m_Author.c_str());
        if(bCurrentRev || bNewRev)
        {
            QFont font = pItem->font();
            if(bCurrentRev)
                font.setBold(true);
            else
                font.setItalic(true);
            pItem->setFont(font);
        }
        pItem->setTextAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
        lineItems.append(pItem);

        QString strDescription(it->m_Description.c_str());
        if(strDescription.contains("\n"))
        {
            strDescription = strDescription.mid(0, strDescription.indexOf("\n"));
            strDescription += "...";
        }
        pItem = new QStandardItem(QString("   ") + strDescription);
        if(bCurrentRev || bNewRev)
        {
            QFont font = pItem->font();
            if(bCurrentRev)
                font.setBold(true);
            else
                font.setItalic(true);
            pItem->setFont(font);
        }
        pItem->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        lineItems.append(pItem);

        bool bFiltered = strFilter.isEmpty();
        for(QList<QStandardItem*>::Iterator lineItemIt = lineItems.begin(); !bFiltered && lineItemIt != lineItems.end(); ++lineItemIt)
        {
            bFiltered = (*lineItemIt)->text().contains(strFilter, Qt::CaseInsensitive);
        }

        if(bFiltered)
        {
            modelRevisions->appendRow(lineItems);
        }
    }

    ui->revisionsTable->resizeColumnsToContents();
    ui->revisionsTable->horizontalHeader()->resizeSection(0, ui->revisionsTable->horizontalHeader()->sectionSize(0) + 40);
    ui->revisionsTable->horizontalHeader()->resizeSection(1, ui->revisionsTable->horizontalHeader()->sectionSize(1) + 40);
    ui->revisionsTable->horizontalHeader()->resizeSection(2, ui->revisionsTable->horizontalHeader()->sectionSize(2) + 40);
    ui->revisionsTable->horizontalHeader()->setStretchLastSection(true);

    //restore selection
    bool selectionRestored = false;
    for(int i = 0; i < modelRevisions->rowCount() && !strSelectedRevision.isEmpty(); i++)
    {
        if(modelRevisions->item(i, 0)->text() == strSelectedRevision)
        {
            ui->revisionsTable->selectRow(i);
            selectionRestored = true;
            break;
        }
    }

    if(!selectionRestored && modelRevisions->rowCount())
    {
        ui->revisionsTable->selectRow(0);
        on_revisionsTable_clicked(modelRevisions->index(0,0));
    }

    std::string repoPath = SvnViewer::instance()->getRepoPath();
    AppSettings::instance()->addToHistory(repoPath);

    ui->labelLogsTitle->setText(QString("Revisions list for <") + m_currentRepoPath + ">");
}

void MainWindow::displayAffectedItems()
{
    int nRevision = getSelectedRevision();
    if(nRevision == -1)
    {
        return;
    }

    RevisionInfo changeset;
    if(!SvnViewer::instance()->getChangeSet(nRevision, changeset))
    {
        return;
    }

    std::stringstream ss;
    ss << changeset.m_No;

    if(ui->lebelRevision->text() == ss.str().c_str() && (modelAffectedItems->rowCount() || changeset.m_AffectedItems.empty()))
    {
        return;
    }

    ui->revisionDetails->setVisible(false);

    QStringList lineItems;
    if(changeset.m_AffectedItems.empty())
    {
        std::stringstream ss; ss << "Loading affected items for revision " << changeset.m_No << " ...";
        lineItems << ss.str().c_str();
        ui->revisionDetails->setEnabled(false);
    }
    else
    {
        std::stringstream ss; ss << "Affected items";
        lineItems << ss.str().c_str();
        ui->revisionDetails->setEnabled(true);
    }

    ui->lebelRevision->setText(ss.str().c_str());
    ui->lebelDate->setText(changeset.m_Date.c_str());
    ui->lebelAuthor->setText(changeset.m_Author.c_str());
    ui->lebelDescription->setText(changeset.m_Description.c_str());

    modelAffectedItems->setHorizontalHeaderLabels(lineItems);

    if( modelAffectedItems->rowCount())
    {
        modelAffectedItems->removeRows(0, modelAffectedItems->rowCount());
    }


    for(std::list<std::string>::const_iterator it = changeset.m_AffectedItems.begin(); it != changeset.m_AffectedItems.end(); ++it)
    {
        modelAffectedItems->appendRow(new QStandardItem(it->c_str()));
    }

    ui->revisionDetails->resizeColumnsToContents();
    ui->revisionDetails->horizontalHeader()->setStretchLastSection(true);
    ui->revisionDetails->setVisible(true);
}


void MainWindow::displayLocalChanges()
{
    ChangeInfo::Collection localChanges = SvnViewer::instance()->getLocalChanges();
    if(m_activeStatusDialog)
    {
        m_activeStatusDialog->updateChanges(localChanges);
    }

    if(m_activeCommitDialog)
    {
        m_activeCommitDialog->updateChanges(localChanges);
    }

    //update status in the local repo tree
    QTreeWidgetItemIterator  it(ui->treeWidgetRepo);
    while (*it)
    {
        updateTreeItemState(*it, localChanges);
        ++it;
    }
}

void MainWindow::fillParentItem(RepoItemInfo::SmartPtr& repoItem, QTreeWidgetItem* parentItem, QTreeWidget* parentView)
{
    QTreeWidgetItem* pItem = parentItem == nullptr ? new QTreeWidgetItem(parentView) : new QTreeWidgetItem(parentItem);
    QString strItemText(repoItem->m_name.c_str());
    pItem->setText(0, strItemText);
    if(repoItem->m_type == RepoItemInfo::Directory)
        pItem->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);

    fillChildItems(repoItem, pItem);
}

void MainWindow::fillChildItems(RepoItemInfo::SmartPtr& repoItem, QTreeWidgetItem* parentItem)
{
    for(RepoItemInfo::SmartPtr& child : repoItem->m_subItems)
    {
        fillParentItem(child, parentItem, nullptr);
    }
}

void MainWindow::updateTreeItemState(QTreeWidgetItem* pTreeItem, ChangeInfo::Collection& localChanges)
{
    QString pathToRoot = getPathToRoot(pTreeItem);
    ChangeInfo::Collection::const_iterator itChanges = localChanges.begin();
    for(; itChanges != localChanges.end(); ++itChanges)
    {
        if(QString(itChanges->m_AffectedItem.c_str()).contains(pathToRoot, Qt::CaseSensitive))
        {
            break;
        }
    }

    QFont font;
    if(itChanges != localChanges.end())
    {
        font.setItalic(true);
    }
    pTreeItem->setFont(0, font);

    static QIcon okIcon(":/Icons/ok.png");
    static QIcon editedIcon(":/Icons/edited.png");
    static QIcon folderOkIcon(":/Icons/folder-ok.png");
    static QIcon folderEditedIcon(":/Icons/folder-edited.png");

    if(pTreeItem->childIndicatorPolicy() == QTreeWidgetItem::ShowIndicator)
    {
        pTreeItem->setIcon(0, itChanges == localChanges.end() ? folderOkIcon : folderEditedIcon);
    }
    else
    {
        pTreeItem->setIcon(0, itChanges == localChanges.end() ? okIcon : editedIcon);
    }
}

void MainWindow::displayRepoContent()
{
    RepoItemInfo::SmartPtr repoContent = SvnViewer::instance()->getRepoContent();
    if(!ui->treeWidgetRepo->topLevelItem(0))
    {
        QTreeWidgetItem* pRootItem = nullptr;
        fillParentItem(repoContent, pRootItem, ui->treeWidgetRepo);
        ui->treeWidgetRepo->addTopLevelItem(pRootItem);
    }
    else
    {
        QTreeWidgetItemIterator  it(ui->treeWidgetRepo);
        while (*it)
        {
            if(!(*it)->childCount())
            {
                QString strItemPath = getPathToRoot(*it);
                RepoItemInfo::SmartPtr childContent = repoContent->findChildNode(strItemPath.toStdString());
                if(childContent && !childContent->m_subItems.empty())
                {
                    fillChildItems(childContent, *it);
                }
            }

            ++it;
        }
    }

    //update status icons also
    displayLocalChanges();
}
