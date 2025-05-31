#ifndef CONTENT_LINE_UTIL_H
#define CONTENT_LINE_UTIL_H

#include <QPalette>

class ChatLineContentUtils
{
public:
    static constexpr qreal borderRadius(){
        return 4.0;
    }
    static QColor foreground();
    static QColor background();
    static QColor alternatedForegound();
    static QColor alternatedBackground();
    static void reloadTheme();

};

#endif  // !CONTENT_LINE_UTIL_H
