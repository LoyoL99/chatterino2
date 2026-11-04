// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/I18n.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>

namespace chatterino {

I18n &I18n::instance()
{
    static I18n inst;
    return inst;
}

bool I18n::loadLanguage(const QString &langCode)
{
    if (langCode == this->currentLang_ && this->isLoaded_)
    {
        return true;
    }

    if (this->isLoaded_)
    {
        QCoreApplication::removeTranslator(&this->appTranslator_);
        this->isLoaded_ = false;
    }

    this->currentLang_ = langCode;

    if (langCode == "en")
    {
        this->qmData_.clear();
        return true;
    }

    QString resPath =
        QStringLiteral(":/translations/chatterino_%1.qm").arg(langCode);
    bool ok = this->appTranslator_.load(resPath);
    if (!ok)
    {
        ok = this->appTranslator_.load(
            QStringLiteral("chatterino_%1.qm").arg(langCode),
            QStringLiteral(":/translations"));
    }

    if (!ok)
    {
        QFile resFile(resPath);
        if (resFile.open(QIODevice::ReadOnly))
        {
            this->qmData_ = resFile.readAll();
            ok = this->appTranslator_.load(
                reinterpret_cast<const uchar *>(this->qmData_.constData()),
                this->qmData_.size());
        }
    }

    if (ok)
    {
        QCoreApplication::installTranslator(&this->appTranslator_);
        this->isLoaded_ = true;
        return true;
    }

    this->qmData_.clear();
    qWarning() << "Failed to load translation resource for" << langCode
               << "from" << resPath;
    return false;
}

QString I18n::currentLanguage() const
{
    return this->currentLang_;
}

}  // namespace chatterino
