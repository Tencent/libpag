#ifndef LICENSEDIALOG_H
#define LICENSEDIALOG_H

#include <QDialog>
#include <QWidget>

class LicenseDialog : public QDialog {
  Q_OBJECT
public:
  explicit LicenseDialog(QWidget *parent = nullptr);
  ~LicenseDialog() override;

  auto init() -> void;

  static auto requestUserAgreement() -> bool;
  static auto isUserAgreeWithLicense() -> bool;
  static auto setUserDisagreeWithLicense() -> void;

  static QString licenseUrl;
  static QString privacyUrl;

private:
};

#endif // LICENSEDIALOG_H