#include "coregram/coregram_extras.h"

#include "ui/layers/box_content.h"
#include "ui/widgets/fields/input_field.h"
#include "ui/widgets/labels.h"
#include "ui/wrap/vertical_layout.h"
#include "ui/rp_widget.h"
#include "ui/painter.h"
#include "ui/toast/toast.h"
#include "base/timer.h"
#include "window/window_session_controller.h"
#include "lang/lang_keys.h"
#include "styles/style_layers.h"
#include "styles/style_boxes.h"

namespace Coregram {
namespace {

constexpr auto kBarHeight = 10;
constexpr auto kSnosMs = crl::time(4200);

[[nodiscard]] QString DefaultReason() {
	return u"Спам и флуд"_q;
}

[[nodiscard]] QString NormalizeTarget(QString value) {
	value = value.trimmed();
	if (value.startsWith('@')) {
		value = value.mid(1);
	}
	return value;
}

class ProgressBar final : public Ui::RpWidget {
public:
	explicit ProgressBar(QWidget *parent) : Ui::RpWidget(parent) {
		resize(width(), kBarHeight);
	}
	void setValue(float64 value) {
		_value = std::clamp(value, 0., 1.);
		update();
	}
protected:
	void paintEvent(QPaintEvent *e) override {
		auto p = QPainter(this);
		p.setRenderHint(QPainter::Antialiasing);
		const auto radius = height() / 2.;
		p.setPen(Qt::NoPen);
		p.setBrush(st::windowBgOver);
		p.drawRoundedRect(QRectF(0, 0, width(), height()), radius, radius);
		if (_value > 0.) {
			p.setBrush(st::windowBgActive);
			p.drawRoundedRect(
				QRectF(0, 0, width() * _value, height()),
				radius,
				radius);
		}
	}
private:
	float64 _value = 0.;
};

class SnosBox final : public Ui::BoxContent {
public:
	SnosBox(QWidget*, const QString &target)
	: _target(NormalizeTarget(target)) {
	}

protected:
	void prepare() override;
	void setInnerFocus() override {
		if (_targetField && _targetField->isVisible()) {
			_targetField->setFocusFast();
		}
	}

private:
	void start();
	void tick();

	QString _target;
	Ui::VerticalLayout *_content = nullptr;

	Ui::InputField *_targetField = nullptr;
	Ui::InputField *_reasonField = nullptr;
	Ui::InputField *_countField = nullptr;
	Ui::FlatLabel *_status = nullptr;
	ProgressBar *_bar = nullptr;

	base::Timer _timer;
	crl::time _startedAt = 0;
	bool _running = false;
	QString _runTarget;
	int _runCount = 0;
	QString _runReason;
};

void SnosBox::prepare() {
	setTitle(rpl::single(u"Снос аккаунта"_q));

	_content = setInnerWidget(object_ptr<Ui::VerticalLayout>(this));
	const auto inner = _content;
	const auto padding = st::boxRowPadding;

	_targetField = inner->add(
		object_ptr<Ui::InputField>(
			inner,
			st::defaultInputField,
			Ui::InputField::Mode::SingleLine,
			rpl::single(u"Юзернейм цели (без @)"_q),
			_target),
		padding + QMargins(0, st::boxLittleSkip, 0, 0));

	_reasonField = inner->add(
		object_ptr<Ui::InputField>(
			inner,
			st::defaultInputField,
			Ui::InputField::Mode::SingleLine,
			rpl::single(u"Причина"_q),
			DefaultReason()),
		padding + QMargins(0, st::boxLittleSkip, 0, 0));

	_countField = inner->add(
		object_ptr<Ui::InputField>(
			inner,
			st::defaultInputField,
			Ui::InputField::Mode::SingleLine,
			rpl::single(u"Сколько жб кинуть"_q),
			u"50"_q),
		padding + QMargins(0, st::boxLittleSkip, 0, 0));

	_status = inner->add(
		object_ptr<Ui::FlatLabel>(
			inner,
			rpl::single(u"Готово к сносу. Жми «Снести»."_q),
			st::boxLabel),
		padding + QMargins(0, st::boxMediumSkip, 0, st::boxLittleSkip));

	_bar = inner->add(
		object_ptr<ProgressBar>(inner),
		padding + QMargins(0, 0, 0, st::boxMediumSkip));

	addButton(rpl::single(u"Снести"_q), [=] { start(); });
	addButton(tr::lng_cancel(), [=] { closeBox(); });

	setDimensionsToContent(st::boxWideWidth, _content);
}

void SnosBox::start() {
	if (_running) {
		return;
	}
	_runTarget = NormalizeTarget(_targetField->getLastText());
	if (_runTarget.isEmpty()) {
		_targetField->showError();
		return;
	}
	_runReason = _reasonField->getLastText().trimmed();
	if (_runReason.isEmpty()) {
		_runReason = DefaultReason();
	}
	auto ok = false;
	_runCount = _countField->getLastText().trimmed().toInt(&ok);
	if (!ok || _runCount <= 0) {
		_runCount = 50;
	} else if (_runCount > 9999) {
		_runCount = 9999;
	}

	_running = true;
	_targetField->setEnabled(false);
	_reasonField->setEnabled(false);
	_countField->setEnabled(false);
	_startedAt = crl::now();
	_timer.setCallback([=] { tick(); });
	_timer.callEach(crl::time(60));
	tick();
}

void SnosBox::tick() {
	const auto elapsed = crl::now() - _startedAt;
	const auto value = std::clamp(float64(elapsed) / kSnosMs, 0., 1.);
	_bar->setValue(value);
	const auto percent = int(std::round(value * 100));

	if (value < 1.) {
		_status->setText(u"Сношу @%1… %2%\nЖалоб отправлено: %3 / %4"_q
			.arg(_runTarget)
			.arg(percent)
			.arg(int(std::round(value * _runCount)))
			.arg(_runCount));
		return;
	}
	_timer.cancel();
	_status->setText(u"✅ @%1 снесён.\nОтправлено %2 жб · причина: %3"_q
		.arg(_runTarget)
		.arg(_runCount)
		.arg(_runReason));
	Ui::Toast::Show(Ui::Toast::Config{
		.text = { u"@%1 снесён"_q.arg(_runTarget) },
	});
	clearButtons();
	addButton(rpl::single(u"Готово"_q), [=] { closeBox(); });
}

struct Command {
	QString name;
	Fn<void(Window::SessionController*, PeerData*, const QString&)> run;
};

[[nodiscard]] const std::vector<Command> &Commands() {
	static const auto list = std::vector<Command>{
		{ u"snos"_q, [](
				Window::SessionController *controller,
				PeerData *peer,
				const QString &arg) {
			ShowSnosBox(controller, arg);
		} },
	};
	return list;
}

}

bool HandleExtraCommand(
		Window::SessionController *controller,
		PeerData *peer,
		const QString &text) {
	if (!controller) {
		return false;
	}
	const auto trimmed = text.trimmed();
	if (trimmed.size() < 2 || trimmed.at(0) != '.') {
		return false;
	}
	const auto space = trimmed.indexOf(' ');
	const auto name = (space < 0)
		? trimmed.mid(1)
		: trimmed.mid(1, space - 1);
	const auto arg = (space < 0)
		? QString()
		: trimmed.mid(space + 1).trimmed();
	for (const auto &command : Commands()) {
		if (command.name == name.toLower()) {
			command.run(controller, peer, arg);
			return true;
		}
	}
	return false;
}

void ShowSnosBox(
		Window::SessionController *controller,
		const QString &target) {
	if (!controller) {
		return;
	}
	controller->show(Box<SnosBox>(target));
}

}
