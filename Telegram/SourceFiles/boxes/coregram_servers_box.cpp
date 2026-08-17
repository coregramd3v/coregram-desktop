/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "boxes/coregram_servers_box.h"

#include "boxes/coregram_add_server_box.h"
#include "boxes/coregram_server_details_box.h"
#include "coregram/coregram_servers.h"
#include "lang/lang_keys.h"
#include "main/main_account.h"
#include "settings/settings_common.h"
#include "ui/boxes/confirm_box.h"
#include "ui/widgets/buttons.h"
#include "ui/wrap/vertical_layout.h"
#include "styles/style_layers.h"
#include "styles/style_settings.h"

CoreGramServersBox::CoreGramServersBox(
	QWidget*,
	not_null<Main::Account*> account)
: _account(account) {
}

void CoreGramServersBox::prepare() {
	setTitle(rpl::single(u"Servers"_q));

	_list = setInnerWidget(object_ptr<Ui::VerticalLayout>(this));
	rebuild();

	addButton(tr::lng_close(), [=] { closeBox(); });
	setDimensions(st::boxWidth, st::boxMaxListHeight);
}

void CoreGramServersBox::rebuild() {
	if (!_list) {
		return;
	}
	_list->clear();

	const auto current = CoreGram::CurrentServerForAccount(_account);
	for (const auto &server : CoreGram::ListServers()) {
		const auto isCurrent = (current.id == server.id)
			&& (current.host == server.host)
			&& (current.port == server.port);
		const auto title = isCurrent
			? (server.name + u" (current)"_q)
			: server.name;
		const auto button = Settings::AddButtonWithLabel(
			_list,
			rpl::single(title),
			rpl::single(CoreGram::FormatEndpoint(server)),
			st::settingsButtonNoIcon);
		button->setClickedCallback([=] {
			getDelegate()->show(Box<ServerDetailsBox>(
				server,
				[=](const CoreGram::Server &chosen) { connectTo(chosen); },
				[=] { rebuild(); },
				CoreGram::IsRemovableServer(server)
					? Fn<void()>([=] {
						getDelegate()->show(Box<AddServerBox>(
							[=](CoreGram::Server) { rebuild(); },
							server));
					})
					: Fn<void()>(nullptr)));
		});
	}

	Settings::AddButtonWithLabel(
		_list,
		rpl::single(u"Add server"_q),
		rpl::single(QString()),
		st::settingsButtonNoIcon
	)->setClickedCallback([=] {
		getDelegate()->show(Box<AddServerBox>([=](CoreGram::Server) {
			rebuild();
		}));
	});
}

void CoreGramServersBox::connectTo(const CoreGram::Server &server) {
	if (!server.valid()) {
		return;
	}
	const auto current = CoreGram::CurrentServerForAccount(_account);
	if ((current.id == server.id)
		&& (current.host == server.host)
		&& (current.port == server.port)) {
		closeBox();
		return;
	}
	const auto account = _account;
	const auto text = u"Connect to \"%1\" (%2)? You will be logged out and asked to sign in again."_q
		.arg(server.name, CoreGram::FormatEndpoint(server));
	getDelegate()->show(Ui::MakeConfirmBox({
		.text = text,
		.confirmed = [=](Fn<void()> close) {
			CoreGram::ApplyServerToAccount(account, server);
			close();
		},
		.confirmText = u"Connect"_q,
	}));
}
