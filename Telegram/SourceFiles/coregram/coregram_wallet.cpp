/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "coregram/coregram_wallet.h"

#include <QtCore/QCryptographicHash>

namespace CoreGram {
namespace {

// Адрес TON в человекочитаемом виде — 36 байт: тег, воркчейн, 32 байта адреса
// счёта и контрольная сумма CRC-16/XMODEM, всё в base64url. Кошельки ходят с
// тегом 0x51 (non-bounceable), поэтому адрес начинается с «UQ».
constexpr auto kNonBounceableTag = char(0x51);
constexpr auto kBasechain = char(0x00);

[[nodiscard]] QByteArray Crc16XModem(const QByteArray &data) {
	auto crc = quint16(0);
	for (const auto byte : data) {
		crc ^= quint16(uchar(byte)) << 8;
		for (auto i = 0; i != 8; ++i) {
			crc = (crc & 0x8000) ? ((crc << 1) ^ 0x1021) : (crc << 1);
		}
	}
	auto result = QByteArray();
	result.append(char(crc >> 8));
	result.append(char(crc & 0xFF));
	return result;
}

} // namespace

QString WalletAddress(uint64 userId) {
	if (!userId) {
		return QString();
	}
	// Та же строка-namespace и та же схема, что в internal/tonaddr на сервере:
	// адрес обязан совпасть до символа, иначе клиент покажет один кошелёк, а
	// подарки будут лежать на другом.
	const auto key = QByteArray("coregram:ton:wallet:v1:")
		+ QByteArray::number(qulonglong(userId));
	const auto account = QCryptographicHash::hash(
		key,
		QCryptographicHash::Sha256);
	auto raw = QByteArray();
	raw.append(kNonBounceableTag);
	raw.append(kBasechain);
	raw.append(account);
	raw.append(Crc16XModem(raw));
	return QString::fromLatin1(raw.toBase64(QByteArray::Base64UrlEncoding));
}

QString WalletExplorerUrl(const QString &address) {
	return address.isEmpty()
		? QString()
		: (u"https://un1quedev.lol/wallet/"_q + address);
}

} // namespace CoreGram
