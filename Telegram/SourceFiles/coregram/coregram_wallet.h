/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QString>

namespace CoreGram {

// WalletAddress — адрес кошелька пользователя. Считается на устройстве по той
// же схеме, что и на сервере (coregram:ton:wallet:v1:<id> -> sha256 -> адрес
// TON-формата), поэтому отдельный запрос за ним не нужен и адрес совпадает с
// тем, что видит сервер и эксплорер.
[[nodiscard]] QString WalletAddress(uint64 userId);

// WalletExplorerUrl — страница кошелька на сервере: что на нём лежит.
[[nodiscard]] QString WalletExplorerUrl(const QString &address);

} // namespace CoreGram
