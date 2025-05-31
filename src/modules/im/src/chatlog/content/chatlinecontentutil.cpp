#include "chatlinecontentutil.h"
#include "lib/storage/settings/style.h"

QColor ChatLineContentUtils::foreground() {
    return lib::settings::Style::getExtColor("chat.message.color");
}

QColor ChatLineContentUtils::background() {
    return lib::settings::Style::getExtColor("chat.message.background");
}

QColor ChatLineContentUtils::alternatedForegound() {
    return lib::settings::Style::getExtColor("chat.message.self.color");
}

QColor ChatLineContentUtils::alternatedBackground() {
    return lib::settings::Style::getExtColor("chat.message.self.background");
}

void ChatLineContentUtils::reloadTheme() {}
