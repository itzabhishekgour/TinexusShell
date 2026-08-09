import os

main_path = 'src/shell/main.cpp'
with open(main_path, 'r', encoding='utf-8') as f:
    main_content = f.read()

# Fix bounds -> frame methods
main_content = main_content.replace('this->frame().x ', 'this->frame().x() ')
main_content = main_content.replace('this->frame().y ', 'this->frame().y() ')
main_content = main_content.replace('this->frame().width ', 'this->frame().width() ')
main_content = main_content.replace('this->frame().height ', 'this->frame().height() ')
main_content = main_content.replace('this->frame().width/', 'this->frame().width()/')
main_content = main_content.replace('this->frame().height/', 'this->frame().height()/')

# Fix fill_rect to fill_rounded_rect for 3 args
main_content = main_content.replace(
    'painter.fill_rect(this->frame(), txui::theme::SURFACE_ELEVATED, 24);',
    'painter.fill_rounded_rect(this->frame(), 24, txui::theme::SURFACE_ELEVATED);'
)

main_content = main_content.replace(
    'painter.fill_rect({this->frame().x() + this->frame().width()/2 - 16, this->frame().y() + 8, 32, 32}, txui::theme::ACCENT_COLOR, 16);',
    'painter.fill_rounded_rect({this->frame().x() + this->frame().width()/2 - 16, this->frame().y() + 8, 32, 32}, 16, txui::theme::ACCENT_COLOR);'
)

with open(main_path, 'w', encoding='utf-8') as f:
    f.write(main_content)

