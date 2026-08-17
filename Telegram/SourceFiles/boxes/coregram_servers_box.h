/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/layers/box_content.h"
#include "coregram/coregram_servers.h"

namespace Main {
class Account;
} // namespace Main

namespace Ui {
class VerticalLayout;
} // namespace Ui

class CoreGramServersBox : public Ui::BoxContent {
public:
	CoreGramServersBox(QWidget*, not_null<Main::Account*> account);

protected:
	void prepare() override;

private:
	void rebuild();
	void connectTo(const CoreGram::Server &server);

	const not_null<Main::Account*> _account;
	Ui::VerticalLayout *_list = nullptr;

};
