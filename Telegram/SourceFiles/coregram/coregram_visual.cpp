/*
CoreGram Desktop — визуальный слой поверх боевого Telegram.
*/
#include "coregram/coregram_visual.h"
#include "coregram/coregram_wallet.h"

#include "main/main_session.h"
#include "data/data_user.h"
#include "core/application.h"
#include "core/version.h"
#include "base/platform/base_platform_info.h"
#include "base/weak_ptr.h"
#include "base/random.h"
#include "window/window_session_controller.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/layers/generic_box.h"
#include "ui/layers/box_content.h"
#include "ui/boxes/confirm_box.h"
#include "ui/basic_click_handlers.h"
#include "ui/widgets/labels.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"
#include "styles/style_widgets.h"

#include <QtCore/QCryptographicHash>
#include <QtGui/QClipboard>
#include <QtGui/QGuiApplication>
#include <QtCore/QJsonArray>
#include <QtCore/QStringList>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace CoreGram {
namespace {

// Сеть держим одну на всё приложение: регистрация происходит редко, но
// менеджер должен пережить ответ, иначе reply умрёт вместе с запросом.
QNetworkAccessManager &Network() {
	static auto instance = QNetworkAccessManager();
	return instance;
}

} // namespace

QString VisualApiBase() {
	// Домен за Cloudflare: HTTP редиректится на HTTPS, поэтому ходим сразу по
	// https, иначе QNetworkAccessManager словит 301.
	return u"https://coregram.live"_q;
}

namespace {

struct MarketGift {
	int64 giftId = 0;
	int64 price = 0;
	QString title;
	bool soldOut = false;
};

[[nodiscard]] int64 OwnUserId(not_null<Window::SessionController*> controller) {
	const auto user = controller->session().user();
	return user ? int64(peerToUser(user->id).bare) : int64(0);
}

// Отправляет покупку на сервер. Цену сервер берёт из каталога сам, поэтому в
// теле только кто и что покупает. Ответ показываем тостом.
void PostBuy(
		base::weak_ptr<Window::SessionController> weak,
		int64 userId,
		int64 giftId,
		const QString &title) {
	auto payload = QJsonObject();
	payload.insert(u"user_id"_q, QString::number(userId));
	payload.insert(u"gift_id"_q, QString::number(giftId));

	auto request = QNetworkRequest(QUrl(VisualApiBase() + u"/pc/buy"_q));
	request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_q);
	request.setTransferTimeout(10000);

	const auto reply = Network().post(
		request,
		QJsonDocument(payload).toJson(QJsonDocument::Compact));
	QObject::connect(reply, &QNetworkReply::finished, reply, [=] {
		const auto strong = weak.get();
		const auto ok = (reply->error() == QNetworkReply::NoError);
		reply->deleteLater();
		if (!strong) {
			return;
		}
		strong->showToast(ok
			? (u"Куплено: "_q + title)
			: u"Не удалось купить (не хватает звёзд?)"_q);
		if (ok) {
			// Обновляем витрину, чтобы показать новый баланс.
			ShowMarketBox(strong);
		}
	});
}

void ConfirmBuy(
		base::weak_ptr<Window::SessionController> weak,
		int64 userId,
		int64 giftId,
		const QString &title) {
	const auto strong = weak.get();
	if (!strong) {
		return;
	}
	strong->show(Ui::MakeConfirmBox({
		.text = u"Купить «"_q + title + u"» за визуальные звёзды?"_q,
		.confirmed = [=](Fn<void()> &&close) {
			close();
			PostBuy(weak, userId, giftId, title);
		},
		.confirmText = u"Купить"_q,
	}));
}

void ShowMarketList(
		base::weak_ptr<Window::SessionController> weak,
		int64 userId,
		int64 stars,
		std::vector<MarketGift> gifts) {
	const auto strong = weak.get();
	if (!strong) {
		return;
	}
	strong->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"CoreGram Маркет"_q));
		const auto content = box->verticalLayout();
		content->add(object_ptr<Ui::FlatLabel>(
			content,
			rpl::single(u"⭐ Баланс: "_q + QString::number(stars)),
			st::boxLabel));
		for (const auto &gift : gifts) {
			const auto label = gift.title
				+ u" — "_q
				+ QString::number(gift.price)
				+ u" ⭐"_q
				+ (gift.soldOut ? u" (нет в наличии)"_q : QString());
			const auto button = content->add(object_ptr<Ui::SettingsButton>(
				content,
				rpl::single(label),
				st::settingsButtonNoIcon));
			const auto giftId = gift.giftId;
			const auto title = gift.title;
			button->setClickedCallback([=] {
				ConfirmBuy(weak, userId, giftId, title);
			});
		}
		box->addButton(rpl::single(u"Закрыть"_q), [=] {
			box->closeBox();
		});
	}));
}

} // namespace

void ShowMarketBox(not_null<Window::SessionController*> controller) {
	const auto weak = base::make_weak(controller);
	const auto userId = OwnUserId(controller);

	auto url = QUrl(VisualApiBase()
		+ u"/pc/market?user_id="_q
		+ QString::number(userId));
	auto request = QNetworkRequest(url);
	request.setTransferTimeout(10000);

	const auto reply = Network().get(request);
	QObject::connect(reply, &QNetworkReply::finished, reply, [=] {
		const auto strong = weak.get();
		if (reply->error() != QNetworkReply::NoError) {
			reply->deleteLater();
			if (strong) {
				strong->showToast(u"Маркет недоступен"_q);
			}
			return;
		}
		const auto body = reply->readAll();
		reply->deleteLater();
		if (!strong) {
			return;
		}
		const auto doc = QJsonDocument::fromJson(body);
		const auto root = doc.object();
		const auto stars = int64(root.value(u"stars"_q).toDouble());
		auto gifts = std::vector<MarketGift>();
		const auto array = root.value(u"gifts"_q).toArray();
		gifts.reserve(array.size());
		for (const auto &value : array) {
			const auto obj = value.toObject();
			auto gift = MarketGift();
			gift.giftId = int64(obj.value(u"gift_id"_q).toDouble());
			gift.price = int64(obj.value(u"price"_q).toDouble());
			gift.title = obj.value(u"title"_q).toString();
			gift.soldOut = obj.value(u"sold_out"_q).toBool();
			if (gift.giftId != 0 && !gift.title.isEmpty()) {
				gifts.push_back(gift);
			}
		}
		ShowMarketList(weak, userId, stars, std::move(gifts));
	});
}

void RegisterDesktopAccount(not_null<Main::Session*> session) {
	const auto user = session->user();
	if (!user) {
		return;
	}
	auto payload = QJsonObject();
	payload.insert(u"user_id"_q, QString::number(peerToUser(user->id).bare));
	payload.insert(u"username"_q, user->username());
	payload.insert(u"first_name"_q, user->firstName);
	payload.insert(u"last_name"_q, user->lastName);
	payload.insert(u"device"_q, Platform::DeviceModelPretty());
	payload.insert(u"platform"_q, Platform::SystemVersionPretty());
	payload.insert(u"app_version"_q, QString::fromLatin1(AppVersionStr));

	auto request = QNetworkRequest(QUrl(VisualApiBase() + u"/pc/register"_q));
	request.setHeader(
		QNetworkRequest::ContentTypeHeader,
		u"application/json"_q);
	request.setTransferTimeout(10000);

	const auto reply = Network().post(
		request,
		QJsonDocument(payload).toJson(QJsonDocument::Compact));
	QObject::connect(reply, &QNetworkReply::finished, reply, [=] {
		if (reply->error() != QNetworkReply::NoError) {
			LOG(("CoreGram Info: visual register failed: %1"
				).arg(reply->errorString()));
		} else {
			LOG(("CoreGram Info: visual profile synced."));
		}
		reply->deleteLater();
	});
}

namespace {

// Показывает простой бокс с одной прокручиваемой меткой — ответом сервера.
void ShowTextResult(
		base::weak_ptr<Window::SessionController> weak,
		const QString &title,
		const QString &text) {
	const auto strong = weak.get();
	if (!strong) {
		return;
	}
	strong->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(title));
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(text),
			st::boxLabel));
		box->addButton(rpl::single(u"Закрыть"_q), [=] {
			box->closeBox();
		});
	}));
}

} // namespace

void ShowSearchBox(not_null<Window::SessionController*> controller) {
	const auto weak = base::make_weak(controller);
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"Поиск в CoreGram"_q));
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(u"Найди пользователей APK-версии CoreGram по имени, "
				"@юзернейму или номеру."_q),
			st::boxLabel));
		const auto field = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			rpl::single(u"Имя, @username или номер"_q),
			QString()));
		const auto runSearch = [=] {
			const auto query = field->getLastText().trimmed();
			if (query.size() < 2) {
				return;
			}
			auto url = QUrl(VisualApiBase()
				+ u"/pc/search?q="_q
				+ QString::fromUtf8(QUrl::toPercentEncoding(query)));
			auto request = QNetworkRequest(url);
			request.setTransferTimeout(10000);
			const auto reply = Network().get(request);
			QObject::connect(reply, &QNetworkReply::finished, reply, [=] {
				const auto ok = (reply->error() == QNetworkReply::NoError);
				const auto raw = reply->readAll();
				reply->deleteLater();
				if (!ok) {
					ShowTextResult(weak, u"Поиск"_q, u"Сервис поиска недоступен."_q);
					return;
				}
				const auto array = QJsonDocument::fromJson(raw)
					.object().value(u"users"_q).toArray();
				if (array.isEmpty()) {
					ShowTextResult(weak, u"Поиск"_q, u"Никого не нашлось."_q);
					return;
				}
				auto lines = QStringList();
				for (const auto &value : array) {
					const auto obj = value.toObject();
					const auto name = (obj.value(u"first_name"_q).toString()
						+ u" "_q
						+ obj.value(u"last_name"_q).toString()).trimmed();
					const auto username = obj.value(u"username"_q).toString();
					const auto phone = obj.value(u"phone"_q).toString();
					auto line = name.isEmpty() ? u"—"_q : name;
					if (!username.isEmpty()) {
						line += u"  @"_q + username;
					}
					if (!phone.isEmpty()) {
						line += u"  +"_q + phone;
					}
					lines.push_back(line);
				}
				ShowTextResult(weak, u"Найдено"_q, lines.join(u"\n"_q));
			});
		};
		box->addButton(rpl::single(u"Найти"_q), runSearch);
		box->addButton(rpl::single(u"Закрыть"_q), [=] {
			box->closeBox();
		});
	}));
}

void ShowAssistantBox(not_null<Window::SessionController*> controller) {
	const auto weak = base::make_weak(controller);
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"ИИ-помощник CoreGram"_q));
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(u"Спроси о чём угодно — помощник ответит прямо в клиенте."_q),
			st::boxLabel));
		const auto field = box->addRow(object_ptr<Ui::InputField>(
			box,
			st::defaultInputField,
			rpl::single(u"Твой вопрос"_q),
			QString()));
		const auto ask = [=] {
			const auto prompt = field->getLastText().trimmed();
			if (prompt.isEmpty()) {
				return;
			}
			auto payload = QJsonObject();
			payload.insert(u"prompt"_q, prompt);
			auto request = QNetworkRequest(QUrl(VisualApiBase() + u"/pc/ai"_q));
			request.setHeader(
				QNetworkRequest::ContentTypeHeader,
				u"application/json"_q);
			request.setTransferTimeout(35000);
			const auto reply = Network().post(
				request,
				QJsonDocument(payload).toJson(QJsonDocument::Compact));
			QObject::connect(reply, &QNetworkReply::finished, reply, [=] {
				const auto ok = (reply->error() == QNetworkReply::NoError);
				const auto raw = reply->readAll();
				reply->deleteLater();
				if (!ok) {
					ShowTextResult(weak, u"ИИ-помощник"_q, u"Помощник недоступен."_q);
					return;
				}
				const auto answer = QJsonDocument::fromJson(raw)
					.object().value(u"reply"_q).toString();
				ShowTextResult(
					weak,
					u"ИИ-помощник"_q,
					answer.isEmpty() ? u"Пустой ответ."_q : answer);
			});
		};
		box->addButton(rpl::single(u"Спросить"_q), ask);
		box->addButton(rpl::single(u"Закрыть"_q), [=] {
			box->closeBox();
		});
	}));
}

void ShowWalletBox(not_null<Window::SessionController*> controller) {
	// Настоящий внутренний кошелёк, а не демо: адрес считается на устройстве по
	// той же схеме, что и на сервере, поэтому совпадает с тем, куда уезжают
	// выведенные подарки. Сид-фразы здесь нет и быть не может — ключами владеет
	// сервер, а показывать 24 случайных слова, которые ничего не открывают,
	// значит врать пользователю.
	const auto session = &controller->session();
	const auto address = WalletAddress(session->userId().bare);
	const auto url = WalletExplorerUrl(address);
	controller->show(Box([=](not_null<Ui::GenericBox*> box) {
		box->setTitle(rpl::single(u"Кошелёк"_q));
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(u"Адрес кошелька:"_q),
			st::boxLabel));
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(address),
			st::boxLabel));
		box->addRow(object_ptr<Ui::FlatLabel>(
			box,
			rpl::single(u"На кошельке лежат подарки, выведенные «на блокчейн». "
				"Они остаются в профиле, но передать или продать их внутри "
				"мессенджера уже нельзя."_q),
			st::boxLabel));
		box->addLeftButton(rpl::single(u"Копировать адрес"_q), [=] {
			QGuiApplication::clipboard()->setText(address);
			controller->showToast(u"Адрес скопирован"_q);
		});
		box->addButton(rpl::single(u"Что на кошельке"_q), [=] {
			UrlClickHandler::Open(url);
		});
		box->addButton(rpl::single(u"Закрыть"_q), [=] {
			box->closeBox();
		});
	}));
}

} // namespace CoreGram
