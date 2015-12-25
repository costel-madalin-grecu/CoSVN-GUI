#include "ChooseRepoDialog.h"
#include "ui_ChooseRepoDialog.h"

#include <QFileDialog>
#include <QTime>
#include <QCoreApplication>

ChooseRepoDialog::ChooseRepoDialog(QWidget *parent, const std::list<std::string>& recents) :
    QDialog(parent),
    ui(new Ui::ChooseRepoDialog)
{
    ui->setupUi(this);

    for(auto text : recents)
    {
        ui->comboBox->addItem(text.c_str());
    }

    if(recents.size())
    {
        ui->comboBox->setCurrentIndex(0);
    }

    ui->comboBox->addItem("Browse...");
}

ChooseRepoDialog::~ChooseRepoDialog()
{
    delete ui;
}

void ChooseRepoDialog::on_buttonBox_accepted()
{
    m_strPath = ui->comboBox->currentText().toStdString();
}

void ChooseRepoDialog::on_buttonBox_rejected()
{
}

void ChooseRepoDialog::on_comboBox_currentIndexChanged(int index)
{
    if(ui->comboBox->itemText(index) == "Browse...")
    {
        QFileDialog dlg(this);
        dlg.setOption(QFileDialog::ShowDirsOnly, true);
        dlg.setFileMode(QFileDialog::Directory);
        if( dlg.exec() == QFileDialog::Accepted && dlg.selectedUrls().size())
        {
            m_strPath = dlg.selectedUrls().front().toLocalFile().toStdString();
            QDialog::accept();

            QCoreApplication::processEvents( QEventLoop::AllEvents, 100 );
        }
        else
        {
            if(ui->comboBox->count())
                ui->comboBox->setCurrentIndex(0);
            else
                ui->comboBox->setCurrentText("");
        }
    }
}
