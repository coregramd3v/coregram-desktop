#!/usr/bin/env python3
"""Смена бренда клиента: CoreGram/AyuGram -> заданное имя.

Зачем отдельный скрипт, а не разовая правка: бренд размазан примерно по
трёмстам местам — исходники, языковые строки, ресурсы Windows, CMake,
инсталлятор. Руками это делается с ошибками и не повторяется при следующем
обновлении с апстрима.

Главное правило: имя меняется ТОЛЬКО внутри строковых литералов и в файлах
сборки. Идентификаторы C++ (CoreGramServersBox, namespace CoreGram,
openCoreGramUrl) и ключи хранилища в нижнем регистре (coregram_servers.json,
"coregram" как id официального сервера) не трогаются никогда: первое сломает
компиляцию, второе — сохранённые сессии и список серверов у людей.

Запуск:
    python Telegram/build/rebrand.py EternalGram
    python Telegram/build/rebrand.py --check      # только показать остатки
"""

import argparse
import os
import re
import sys
import uuid

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

OLD_BRANDS = ("CoreGram", "AyuGram", "OwpenGram", "Owpengram")

SOURCE_DIRS = ("Telegram/SourceFiles",)
SOURCE_EXT = (".cpp", ".h")

PLAIN_FILES = (
    "Telegram/Resources/langs/lang.strings",
    "Telegram/Resources/winrc/Telegram.rc",
    "Telegram/Resources/winrc/Updater.rc",
)

# Строковый литерал C++: "…", u"…", L"…", R-строки тут не встречаются.
LITERAL = re.compile(r'"(?:[^"\\\n]|\\.)*"')

# Ссылки на чужие рабочие сервисы. Переименовывать их нельзя: адрес перестанет
# существовать, и функция тихо отвалится. Пример — репозиторий переводов
# AyuGram: оттуда клиент тянет локализацию своих настроек, своего такого у нас
# нет, а без него часть настроек показывается по-английски.
KEEP_AS_IS = (
    "cdn.jsdelivr.net/gh/AyuGram/Languages",
)


def brand_words(brand):
    """Пары «что менять -> на что» для одного нового имени."""
    out = []
    for old in OLD_BRANDS:
        if old == brand:
            continue
        out.append((old, brand))
    return out


def replace_in_literals(text, pairs):
    def fix(match):
        value = match.group(0)
        if any(keep in value for keep in KEEP_AS_IS):
            return value
        for old, new in pairs:
            value = value.replace(old, new)
        return value

    return LITERAL.sub(fix, text)


def replace_plain(text, pairs):
    for old, new in pairs:
        text = text.replace(old, new)
    return text


def walk_sources():
    for base in SOURCE_DIRS:
        for dirpath, _, names in os.walk(os.path.join(ROOT, base)):
            for name in names:
                if name.endswith(SOURCE_EXT):
                    yield os.path.join(dirpath, name)


def walk_langs():
    langs = os.path.join(ROOT, "Telegram/Resources/langs")
    for dirpath, _, names in os.walk(langs):
        for name in names:
            if name.endswith(".strings"):
                yield os.path.join(dirpath, name)


def read(path):
    with open(path, "r", encoding="utf-8", errors="surrogateescape") as f:
        return f.read()


def write(path, text):
    with open(path, "w", encoding="utf-8", errors="surrogateescape") as f:
        f.write(text)


def apply_version_header(brand, app_id, old_name):
    path = os.path.join(ROOT, "Telegram/SourceFiles/core/version.h")
    text = read(path)
    text = re.sub(r'AppId = "\{[0-9A-Fa-f-]+\}"_cs;',
                  'AppId = "{%s}"_cs;' % app_id, text)
    text = re.sub(r'AppNameOld = "[^"]*"_cs;',
                  'AppNameOld = "%s"_cs;' % (old_name or (brand + " for Windows")), text)
    text = re.sub(r'AppName = "[^"]*"_cs;',
                  'AppName = "%s Desktop"_cs;' % brand, text)
    text = re.sub(r'AppFile = "[^"]*"_cs;',
                  'AppFile = "%s"_cs;' % brand, text)
    write(path, text)
    return path


def apply_links(releases_channel, releases_url):
    """Проставить настоящие адреса вместо переименованных.

    Механическая замена имени превращает ссылки апстрима в несуществующие
    (github.com/EternalGram/…), поэтому канал новостей и страницу релизов
    задаём явно.
    """
    edits = (
        ("Telegram/SourceFiles/boxes/about_box.cpp",
         r'QString\("@\w+Releases"\)', 'QString("@%s")' % releases_channel),
        ("Telegram/SourceFiles/history/history_item_helpers.cpp",
         r'u"https://t\.me/\w+Releases"_q', 'u"https://t.me/%s"_q' % releases_channel),
        ("Telegram/SourceFiles/core/update_checker.cpp",
         r'"https://t\.me/\w+Releases"', '"https://t.me/%s"' % releases_channel),
        ("Telegram/SourceFiles/core/application.cpp",
         r'u"https://github\.com/[\w./-]+/releases"_q',
         'u"%s"_q' % releases_url),
    )
    touched = []
    for rel, pattern, value in edits:
        path = os.path.join(ROOT, rel)
        text = read(path)
        fixed = re.sub(pattern, value, text)
        if fixed != text:
            write(path, fixed)
            touched.append(path)
    return touched


def apply_cmake(brand):
    path = os.path.join(ROOT, "Telegram/CMakeLists.txt")
    text = read(path)
    text = replace_plain(text, brand_words(brand))
    write(path, text)
    return path


def apply_setup(brand, installer_id, publisher, url):
    path = os.path.join(ROOT, "Telegram/build/setup.iss")
    text = read(path)
    text = replace_plain(text, brand_words(brand))
    text = re.sub(r'#define MyAppPublisher "[^"]*"',
                  '#define MyAppPublisher "%s"' % publisher, text)
    text = re.sub(r'#define MyAppURL "[^"]*"',
                  '#define MyAppURL "%s"' % url, text)
    text = re.sub(r'#define MyAppName "[^"]*"',
                  '#define MyAppName "%s Desktop"' % brand, text)
    text = re.sub(r'#define MyAppId "[0-9A-Fa-f-]+"',
                  '#define MyAppId "%s"' % installer_id, text)
    text = re.sub(r'OutputBaseFilename=\w+setup',
                  'OutputBaseFilename=%ssetup' % brand.lower(), text)
    write(path, text)
    return path


def run(brand, app_id, installer_id, publisher, url, releases_channel, releases_url, old_name):
    pairs = brand_words(brand)
    touched = []

    for path in walk_sources():
        text = read(path)
        fixed = replace_in_literals(text, pairs)
        if fixed != text:
            write(path, fixed)
            touched.append(path)

    for path in walk_langs():
        text = read(path)
        fixed = replace_plain(text, pairs)
        if fixed != text:
            write(path, fixed)
            touched.append(path)

    for rel in PLAIN_FILES:
        path = os.path.join(ROOT, rel)
        if not os.path.exists(path):
            continue
        text = read(path)
        fixed = replace_plain(text, pairs)
        if fixed != text:
            write(path, fixed)
            touched.append(path)

    touched.extend(apply_links(releases_channel, releases_url))
    touched.append(apply_version_header(brand, app_id, old_name))
    touched.append(apply_cmake(brand))
    touched.append(apply_setup(brand, installer_id, publisher, url))
    return sorted(set(touched))


def check(brand=None):
    """Показать оставшиеся чужие имена там, где их видит пользователь.

    В .cpp/.h смотрим только внутрь строковых литералов: имя в комментарии или
    в названии класса ни на что не влияет и переименованию не подлежит.
    """
    leftovers = {}

    def note(old, path, number, line):
        leftovers.setdefault(old, []).append(
            (os.path.relpath(path, ROOT), number, line.strip()[:120]))

    for path in walk_sources():
        for number, line in enumerate(read(path).splitlines(), 1):
            visible = " ".join(LITERAL.findall(line))
            if not visible or any(keep in visible for keep in KEEP_AS_IS):
                continue
            for old in OLD_BRANDS:
                if (brand and old == brand) or old not in visible:
                    continue
                note(old, path, number, line)

    others = list(walk_langs())
    for rel in PLAIN_FILES + ("Telegram/CMakeLists.txt", "Telegram/build/setup.iss"):
        path = os.path.join(ROOT, rel)
        if os.path.exists(path):
            others.append(path)
    for path in others:
        for number, line in enumerate(read(path).splitlines(), 1):
            for old in OLD_BRANDS:
                if (brand and old == brand) or old not in line:
                    continue
                note(old, path, number, line)
    return leftovers


def main():
    parser = argparse.ArgumentParser(description="Смена бренда клиента")
    parser.add_argument("brand", nargs="?", help="новое имя, например EternalGram")
    parser.add_argument("--check", action="store_true",
                        help="только показать оставшиеся чужие имена")
    parser.add_argument("--app-id", help="GUID приложения; по умолчанию новый")
    parser.add_argument("--installer-id", help="GUID инсталлятора; по умолчанию новый")
    parser.add_argument("--publisher", default="2CORE",
                        help="издатель в инсталляторе")
    parser.add_argument("--url", default="https://github.com/coregramd3v/coregram-desktop",
                        help="сайт продукта в инсталляторе")
    parser.add_argument("--releases", help="канал новостей, по умолчанию <Бренд>Releases")
    parser.add_argument("--old-name",
                        help="каталог прежней сборки в %APPDATA%, откуда перенести "
                             "сессии при первом запуске, например \"CoreGram Desktop\"")
    parser.add_argument("--releases-url",
                        default="https://github.com/coregramd3v/coregram-desktop/releases",
                        help="страница релизов")
    args = parser.parse_args()

    if args.check:
        leftovers = check(args.brand)
        if not leftovers:
            print("Чужих имён не осталось.")
            return 0
        for old, places in leftovers.items():
            print("%s — %d упоминаний:" % (old, len(places)))
            for rel, number, line in places[:20]:
                print("   %s:%d  %s" % (rel, number, line))
            if len(places) > 20:
                print("   … и ещё %d" % (len(places) - 20))
        return 1

    if not args.brand:
        parser.error("нужно имя бренда")
    if not re.fullmatch(r"[A-Za-z][A-Za-z0-9]{2,}", args.brand):
        parser.error("имя латиницей без пробелов, например EternalGram")

    app_id = args.app_id or str(uuid.uuid4()).upper()
    installer_id = args.installer_id or str(uuid.uuid4()).upper()
    releases = args.releases or (args.brand + "Releases")
    touched = run(args.brand, app_id, installer_id, args.publisher, args.url,
                  releases, args.releases_url, args.old_name)
    print("Бренд: %s" % args.brand)
    print("GUID приложения: {%s}" % app_id)
    print("GUID инсталлятора: %s" % installer_id)
    print("Канал новостей: @%s" % releases)
    print("Изменено файлов: %d" % len(touched))
    return 0


if __name__ == "__main__":
    sys.exit(main())
