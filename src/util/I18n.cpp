// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/I18n.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QLibraryInfo>

#include <memory>

namespace chatterino {

I18n &I18n::instance()
{
    static I18n inst;
    return inst;
}

bool I18n::loadLanguage(const QString &langCode)
{
    if (this->appTranslator_)
    {
        QCoreApplication::removeTranslator(this->appTranslator_.get());
        this->appTranslator_.reset();
    }
    if (this->qtTranslator_)
    {
        QCoreApplication::removeTranslator(this->qtTranslator_.get());
        this->qtTranslator_.reset();
    }
    this->appQmBytes_.clear();

    this->currentLang_ = langCode;

    if (langCode == "en")
    {
        return true;
    }

    auto appTrans = std::make_unique<QTranslator>();
    QString resPath = QString(":/translations/chatterino_%1.qm").arg(langCode);
    bool loaded = appTrans->load(resPath);

    if (!loaded)
    {
        QFile resFile(resPath);
        if (resFile.open(QIODevice::ReadOnly))
        {
            this->appQmBytes_ = resFile.readAll();
            loaded = appTrans->load(
                reinterpret_cast<const uchar *>(this->appQmBytes_.constData()),
                this->appQmBytes_.size());
            if (!loaded)
            {
                qWarning() << "Failed to parse translation from raw bytes for"
                           << langCode;
            }
        }
    }

    if (loaded)
    {
        QCoreApplication::installTranslator(appTrans.get());
        this->appTranslator_ = std::move(appTrans);
    }
    else
    {
        this->appQmBytes_.clear();
        qWarning() << "Failed to load translation resource for" << langCode
                   << "from" << resPath;
    }

    auto qtTrans = std::make_unique<QTranslator>();
    if (qtTrans->load(QString("qt_%1").arg(langCode),
                      QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
    {
        QCoreApplication::installTranslator(qtTrans.get());
        this->qtTranslator_ = std::move(qtTrans);
    }

    return this->appTranslator_ != nullptr;
}

QString I18n::currentLanguage() const
{
    return this->currentLang_;
}

}  // namespace chatterino
