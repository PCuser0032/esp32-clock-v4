#pragma once

class Button;  // forward declaration, чтобы не тянуть button.h в заголовок

namespace WebUI {
void begin();
void tick();

// Регистрация кнопок для /api/diag. Вызывать из setup() после
// btn1.begin() / btn2.begin(). Если не вызвать — секция "buttons"
// в /api/diag будет отсутствовать.
void setButtons(Button *b1, Button *b2);
}