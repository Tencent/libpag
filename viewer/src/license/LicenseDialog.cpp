#include "LicenseDialog.h"

#include <QDir>
#include <QFile>
#include <QLabel>
#include <QTextEdit>
#include <QSettings>
#include <QCheckBox>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QPushButton>
#include <QStandardPaths>

QString LicenseDialog::licenseUrl = "http://rule.tencent.com/rule/202501170003";
QString LicenseDialog::privacyUrl = "http://rule.tencent.com/rule/202501170004";

LicenseDialog::LicenseDialog(QWidget* parent) : QDialog(parent) {
  init();
}

LicenseDialog::~LicenseDialog() = default;

auto LicenseDialog::init() -> void {
  QFont font;
  font.setPixelSize(16);
  this->setFont(font);
  QVBoxLayout *mainLayout = new QVBoxLayout(this);

  QString text = QString(tr("<p style='margin-bottom: 10px;'>Welcome to PAGViewer! To better protect your rights and interests, please carefully read the <a href=\"%1\">\"%2\"</a> and <a href=\"%3\">\"%4\"</a> terms before using the application. If you have read and agree to these terms, please check the option below and click the \"%5\" button to start using PAGViewer.</p>"));
  text = text.arg(LicenseDialog::licenseUrl).arg(tr("Software License and Service Agreement")).arg(LicenseDialog::privacyUrl).arg(tr("Privacy Protection Statement")).arg(tr("Confirm"));

  QLabel *textLabel = new QLabel(this);
  textLabel->setWordWrap(true);
  textLabel->setTextFormat(Qt::RichText);
  textLabel->setOpenExternalLinks(true);
  textLabel->setStyleSheet(
    "QLabel {"
    "    line-height: 1.5;"
    "    color: #333333;"
    "}"
    "QLabel a {"
    "    color: #0066cc;"
    "    text-decoration: none;"
    "}"
    "QLabel a:hover {"
    "    text-decoration: underline;"
    "}"
  );
  textLabel->setText(text);
  textLabel->setFont(font);
  mainLayout->addWidget(textLabel);

  QCheckBox *agreeCheckBox = new QCheckBox(tr("I have read and agree to the above terms"), this);
  agreeCheckBox->setFont(font);
  mainLayout->addWidget(agreeCheckBox);

  QHBoxLayout *buttonLayout = new QHBoxLayout();
  QPushButton *agreeButton = new QPushButton(tr("Confirm"), this);
  agreeButton->setStyleSheet(R"(
    QPushButton {
        color: white;
        background-color: #0063CC;
        border: none;
        border-radius: 6px;
        padding: 2px 16px 2px 16px;
        background: qlineargradient(
            x1:0, y1:0, x2:0, y2:1,
            stop:0 #0A84FF,
            stop:1 #0066FF
        );
        border: 1px solid #0058D6;
    }
    QPushButton:hover {
        background: qlineargradient(
            x1:0, y1:0, x2:0, y2:1,
            stop:0 #1E8EFF,
            stop:1 #0070FF
        );
    }
    QPushButton:pressed {
        background: qlineargradient(
            x1:0, y1:0, x2:0, y2:1,
            stop:0 #0060E6,
            stop:1 #0052CC
        );
    }
    QPushButton:disabled {
        background: qlineargradient(
            x1:0, y1:0, x2:0, y2:1,
            stop:0 #99C5FF,
            stop:1 #99B3FF
        );
        border: 1px solid #B3D1FF;
        color: rgba(255, 255, 255, 0.5);
    }
    QPushButton:focus {
        outline: none;
        box-shadow: 0 0 0 4px rgba(0, 122, 255, 0.3);
    }
   )");
  agreeButton->setFont(font);
  agreeButton->setEnabled(false);
  agreeButton->setDefault(true);
  QPushButton *disagreeButton = new QPushButton(tr("Close"), this);
  disagreeButton->setStyleSheet(R"(
    QPushButton {
        color: #000000;
        background-color: #FFFFFF;
        border: none;
        border-radius: 6px;
        padding: 2px 16px;
        background: qlineargradient(
            x1:0, y1:0, x2:0, y2:1,
            stop:0 #FFFFFF,
            stop:1 #F0F0F0
        );
        border: 1px solid #D9D9D9;
    }
    QPushButton:hover {
        background: qlineargradient(
            x1:0, y1:0, x2:0, y2:1,
            stop:0 #F5F5F5,
            stop:1 #E8E8E8
        );
    }
    QPushButton:pressed {
        background: qlineargradient(
            x1:0, y1:0, x2:0, y2:1,
            stop:0 #E6E6E6,
            stop:1 #DCDCDC
        );
    }
    QPushButton:disabled {
        background: #F5F5F5;
        border: 1px solid #E0E0E0;
        color: #999999;
    }
    QPushButton:focus {
        outline: none;
        box-shadow: 0 0 0 4px rgba(0, 0, 0, 0.1);
    }
  )");
  disagreeButton->setFont(font);
  buttonLayout->addStretch();
  buttonLayout->addWidget(agreeButton);
  buttonLayout->addWidget(disagreeButton);
  mainLayout->addLayout(buttonLayout);

  connect(agreeCheckBox, &QCheckBox::checkStateChanged, [=] (int state) {
    agreeButton->setEnabled(state == Qt::Checked);
  });
  connect(agreeButton, &QPushButton::clicked, this, &QDialog::accept);
  connect(disagreeButton, &QPushButton::clicked, this, &QDialog::reject);

  setLayout(mainLayout);
  setWindowTitle(tr("Agreement and Statement"));
#if defined(__APPLE__)
  setMinimumSize(QSize(290, 230));
  resize(380, 280);
#elif defined(WIN32)
  setMinimumSize(QSize(300, 230));
  resize(400, 280);
#endif
}

auto LicenseDialog::isUserAgreeWithLicense() -> bool {
  QString settingPath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("setting.ini");
  QSettings settings(settingPath, QSettings::IniFormat);
  return settings.value("UserAgreeWithLicense", false).toBool();
}

auto LicenseDialog::requestUserAgreement() -> bool {
  LicenseDialog dialog;

  if (dialog.exec() == QDialog::Accepted) {
    QString settingPath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("setting.ini");
    QSettings settings(settingPath, QSettings::IniFormat);
    settings.setValue("UserAgreeWithLicense", true);
    return true;
  }

  return false;
}

auto LicenseDialog::setUserDisagreeWithLicense() -> void {
  QString settingPath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("setting.ini");
  QSettings settings(settingPath, QSettings::IniFormat);
  settings.setValue("UserAgreeWithLicense", false);
}
