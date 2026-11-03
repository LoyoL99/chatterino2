// SPDX-FileCopyrightText: 2026 Contributors to Chatterino <https://chatterino.com>
//
// SPDX-License-Identifier: MIT

#include "util/I18n.hpp"

#include <QCoreApplication>
#include <QDebug>
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

    this->currentLang_ = langCode;

    if (langCode == "en")
    {
        return true;
    }

    auto appTrans = std::make_unique<QTranslator>();
    if (appTrans->load(
            QString(":/translations/chatterino_%1.qm").arg(langCode)))
    {
        QCoreApplication::installTranslator(appTrans.get());
        this->appTranslator_ = std::move(appTrans);
    }
    else
    {
        qWarning() << "Failed to load translation for" << langCode;
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
