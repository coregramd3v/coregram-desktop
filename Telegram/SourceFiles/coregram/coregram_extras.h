#pragma once

class PeerData;

namespace Window {
class SessionController;
} Window

namespace Coregram {

[[nodiscard]] bool HandleExtraCommand(
	Window::SessionController *controller,
	PeerData *peer,
	const QString &text);

void ShowSnosBox(
	Window::SessionController *controller,
	const QString &target);

}
