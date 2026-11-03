// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTranslator>

#include <memory>

namespace chatterino {

class I18n : public QObject
{
    Q_OBJECT

public:
    static I18n &instance();

    I18n(const I18n &) = delete;
    I18n &operator=(const I18n &) = delete;
    I18n(I18n &&) = delete;
    I18n &operator=(I18n &&) = delete;

    bool loadLanguage(const QString &langCode);
    QString currentLanguage() const;

private:
    I18n() = default;
    ~I18n() override = default;

    std::unique_ptr<QTranslator> appTranslator_;
    std::unique_ptr<QTranslator> qtTranslator_;
    QByteArray appQmBytes_;
    QString currentLang_{"en"};
};

}  // namespace chatterino
