#ifndef CHOOSEREPODIALOG_H
#define CHOOSEREPODIALOG_H

#include <QDialog>


namespace Ui {
class ChooseRepoDialog;
}

class ChooseRepoDialog : public QDialog
{
    Q_OBJECT

public:
    ChooseRepoDialog(QWidget *parent, const std::list<std::string>& recents);
    ~ChooseRepoDialog();

    std::string getSelectedPath() const { return m_strPath; }
private slots:
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();

    void on_comboBox_currentIndexChanged(int index);

private:
    Ui::ChooseRepoDialog *ui;
    std::string m_strPath;
};

#endif // CHOOSEREPODIALOG_H
