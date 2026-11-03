<div align="center">

![Chatterino Logo](https://user-images.githubusercontent.com/41973452/272541622-52457e89-5f16-4c83-93e7-91866c25b606.png)

# Chatterino 2 — Russian Localization Edition

**Кастомная версия популярного чат-клиента для Twitch, Kick и IRC с полноценным русским интерфейсом и динамическим переключением языка.**

[![Build Status](https://github.com/LoyoL99/chatterino2/actions/workflows/build.yml/badge.svg?branch=feat%2Frussian-localization)](https://github.com/LoyoL99/chatterino2/actions/workflows/build.yml?query=branch%3Afeat%2Frussian-localization)
[![GitHub release](https://img.shields.io/github/v/release/LoyoL99/chatterino2?include_prereleases&label=release&color=blue)](https://github.com/LoyoL99/chatterino2/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-blue.svg)](https://github.com/LoyoL99/chatterino2)
[![Qt](https://img.shields.io/badge/Qt-6.8%2B-green.svg)](https://www.qt.io/)
[![C++](https://img.shields.io/badge/C%2B%2B-20%2F23-purple.svg)](https://isocpp.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

[English description](#english-overview) • [Скачать сборку](#-скачивание-и-установка) • [Возможности](#-особенности-русской-версии) • [Для разработчиков](#-для-разработчиков-и-переводчиков)

</div>

---

## 🇷🇺 Особенности русской версии

Данный репозиторий является форком [Chatterino 2](https://github.com/Chatterino/chatterino2) и развивает нативную подсистему локализации для русскоязычных пользователей и стримеров.

- **Полная русификация ключевых разделов**:
  - Окно настроек (**Настройки** ➔ **Общие**, **Внешний вид**, **Модерация**, **Счетчики** и др.).
  - Меню вкладок, контекстные меню сплитов и чата.
  - Карточки информации о пользователях (**UserInfoPopup**) и модераторские действия (таймаут, бан, удаление сообщений).
  - Диалоговые окна, подсказки и системные кнопки (*ОК*, *Отмена*, *Применить*).
- **Динамическое переключение языка**: переключение между `Русский` и `English` доступно прямо в настройках (`Настройки -> Общие -> Язык интерфейса`) без необходимости перезапускать программу.
- **Отказоустойчивость**: если перевод для новой функции ещё не добавлен, интерфейс прозрачно отображает английский оригинал без сбоев и ошибок.
- **Современный стек**: сборка на базе **Qt 6.8+**, **C++20/C++23** и **MSVC 2022**.
- **Автоматизированный CI/CD**: каждая сборка автоматически компилируется и проверяется в облаке через GitHub Actions.

---

## 📥 Скачивание и установка

### Способ 1: Готовая сборка из GitHub Actions (Рекомендуется)
Каждый коммит в ветку `feat/russian-localization` автоматически собирается в облаке:
1. Перейдите во вкладку [**Actions ➔ Build**](https://github.com/LoyoL99/chatterino2/actions/workflows/build.yml?query=branch%3Afeat%2Frussian-localization).
2. Выберите самый свежий успешный запуск с зеленой галочкой.
3. В нижней части страницы в секции **Artifacts** скачайте архив `chatterino-windows-x86-64-Qt-6.8.3.zip`.
4. Распакуйте архив в удобное место и запустите `chatterino.exe`.

### Способ 2: GitHub Releases
Готовые релизные архивы также публикуются на странице [**Releases**](https://github.com/LoyoL99/chatterino2/releases).

> [!NOTE]
> Для запуска программы требуется установленный пакет [Microsoft Visual C++ Redistributable 2015–2022 x64](https://aka.ms/vs/17/release/vc_redist.x64.exe).

---

## ⚙️ Быстрый старт

1. Запустите `chatterino.exe`.
2. Откройте настройки горячей клавишей `Ctrl + P` (или через иконку шестеренки).
3. Перейдите в раздел **Общие** (General) ➔ найдите пункт **Язык интерфейса** (UI Language) ➔ выберите **Русский**.
4. Добавьте ваш Twitch-аккаунт в разделе **Учетные записи** (Accounts) и настройте нужные каналы.

---

## 🛠️ Для разработчиков и переводчиков

Архитектура локализации построена вокруг менеджера `chatterino::I18n` и стандартной системы интернационализации Qt (`QTranslator`, `.ts` и `.qm` файлы).

### Структура файлов перевода:
- Исходный XML-файл перевода: `resources/translations/chatterino_ru.ts`
- Скомпилированный бинарный ресурс: `resources/translations/chatterino_ru.qm` (встраивается в Qt-ресурсы по пути `:/translations/chatterino_ru.qm`)
- Модуль локализации: `src/util/I18n.hpp` и `src/util/I18n.cpp`

### Как дополнить или отредактировать перевод:
1. Откройте `resources/translations/chatterino_ru.ts` в **Qt Linguist** или любом текстовом редакторе.
2. Добавьте или отредактируйте строку в нужном контексте:
   ```xml
   <context>
       <name>SplitHeader</name>
       <message>
           <source>Reload channel</source>
           <translation>Перезагрузить канал</translation>
       </message>
   </context>
   ```
3. Скомпилируйте бинарный словарь `.qm`:
   ```powershell
   lrelease resources/translations/chatterino_ru.ts -qm resources/translations/chatterino_ru.qm
   ```
4. Закоммитьте изменения и отправьте Pull Request в ветку `feat/russian-localization`.

### Сборка из исходного кода
Подробные руководства по сборке клиента:
- [Сборка на Windows (CMake + Conan)](BUILDING_ON_WINDOWS.md)
- [Сборка на Windows с помощью vcpkg](BUILDING_ON_WINDOWS_WITH_VCPKG.md)
- [Сборка на Linux](BUILDING_ON_LINUX.md)
- [Сборка на macOS](BUILDING_ON_MAC.md)

---

## 🌐 English Overview

**Chatterino 2 — Russian Localization Edition** is a specialized fork of Chatterino 2 providing complete Russian translation support for Twitch and Kick chat users.

- Dynamic runtime language switching between Russian and English.
- Native Qt 6.8+ translation pipeline (`QTranslator` + fail-safe fallbacks).
- Portable Windows x64 distribution available via automated GitHub Actions CI.
- For upstream Chatterino 2 documentation, visit the [official wiki](https://wiki.chatterino.com).

---

## 📜 Лицензия и благодарности

- Проект распространяется под лицензией [MIT License](LICENSE).
- Оригинальный проект: [Chatterino 2](https://github.com/Chatterino/chatterino2) от team Chatterino.
- Локализация и сопровождение ветки: [@LoyoL99](https://github.com/LoyoL99).
