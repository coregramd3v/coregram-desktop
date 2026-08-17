/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "ui/layers/box_content.h"
#include "coregram/coregram_servers.h"

namespace Ui {
class FlatLabel;
class RoundButton;
class RpWidget;
class VerticalLayout;
} // namespace Ui

class ServerDetailsBox : public Ui::BoxContent {
public:
	ServerDetailsBox(
		QWidget*,
		CoreGram::Server server,
		Fn<void(const CoreGram::Server&)> connect,
		Fn<void()> removed = nullptr,
		Fn<void()> edit = nullptr);

protected:
	void prepare() override;

private:
	CoreGram::Server _server;
	Fn<void(const CoreGram::Server&)> _connect;
	Fn<void()> _removed;
	Fn<void()> _edit;
	object_ptr<Ui::VerticalLayout> _content;
	QPointer<Ui::RpWidget> _statusBlock;

};
