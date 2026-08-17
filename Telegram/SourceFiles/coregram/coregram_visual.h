/*
CoreGram Desktop — визуальный слой поверх боевого Telegram.
*/
#pragma once

class QString;

namespace Main {
class Session;
} // namespace Main

namespace Window {
class SessionController;
} // namespace Window

namespace CoreGram {

// Сообщает нашему серверу, что пользователь зашёл с десктопа: реальный
// telegram id, юзернейм и устройство. Аккаунт после этого появляется в
// админ-панели, и ему можно выдавать визуальные номера, подарки и звёзды.
//
// Вызов неблокирующий и молчаливый: недоступность сервера не должна мешать
// работе мессенджера, поэтому ошибки только пишутся в лог.
void RegisterDesktopAccount(not_null<Main::Session*> session);

// Базовый адрес косметического сервера.
[[nodiscard]] QString VisualApiBase();

// Открывает бокс «CoreGram Маркет»: тянет каталог визуальных подарков с нашего
// сервера, показывает баланс визуальных звёзд и позволяет купить подарок за них.
// Всё это существует только в CoreGram — в обычном Telegram покупки не видно.
void ShowMarketBox(not_null<Window::SessionController*> controller);

// Поиск пользователей APK-версии CoreGram: десктоп на боевом Telegram их не
// видит, поэтому ищет через наш сервер (/pc/search).
void ShowSearchBox(not_null<Window::SessionController*> controller);

// Встроенный ИИ-помощник: отправляет вопрос на /pc/ai и показывает ответ.
void ShowAssistantBox(not_null<Window::SessionController*> controller);

// CoreCrypto — локальный некастодиальный кошелёк (демо): генерирует сид-фразу на
// устройстве и показывает её. Приватный ключ наружу не уходит.
void ShowWalletBox(not_null<Window::SessionController*> controller);

} // namespace CoreGram
