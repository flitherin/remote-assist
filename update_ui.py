from pathlib import Path
p=Path(__file__).parent/'src/window.c'
s=p.read_text()
s=s.replace('static HFONT body_font, title_font;', 'static HWND address_label, address_edit, port_label, port_edit, notice;\nstatic HFONT body_font, title_font;')
s=s.replace('MoveWindow(status_label,px(28),px(144),r.right-px(56),px(46),TRUE);', '''MoveWindow(address_label,px(28),px(142),px(320),px(24),TRUE);
    MoveWindow(address_edit,px(28),px(170),px(350),px(32),TRUE);
    MoveWindow(port_label,px(398),px(142),px(100),px(24),TRUE);
    MoveWindow(port_edit,px(398),px(170),px(110),px(32),TRUE);
    MoveWindow(notice,px(28),px(218),r.right-px(56),px(46),TRUE);
    MoveWindow(status_label,px(28),px(290),r.right-px(56),px(46),TRUE);''')
s=s.replace('px(218),px(270)', 'px(362),px(270)').replace('px(218),px(120)', 'px(362),px(120)')
s=s.replace('children[]={description,status_label,confirm_button,stop_button}', 'children[]={description,status_label,confirm_button,stop_button,address_label,address_edit,port_label,port_edit,notice}')
s=s.replace('Подключение к компьютеру','Рабочее место оператора')
s=s.replace('        status_label=control', '''#ifdef ASSIST_HOST
        address_label=control(window,L"STATIC",L"Адрес сервера · предпросмотр",0,0);
        address_edit=control(window,L"EDIT",L"127.0.0.1",WS_BORDER|ES_READONLY|WS_TABSTOP,3);
#else
        address_label=control(window,L"STATIC",L"Адрес компьютера · предпросмотр",0,0);
        address_edit=control(window,L"EDIT",L"127.0.0.1",WS_BORDER|ES_READONLY|WS_TABSTOP,3);
#endif
        port_label=control(window,L"STATIC",L"Порт",0,0);
        port_edit=control(window,L"EDIT",L"—",WS_BORDER|ES_READONLY,4);
        notice=control(window,L"STATIC",L"Только локальная демонстрация.\nСетевой транспорт отсутствует; адрес не используется.",0,0);
        status_label=control''')
s=s.replace('case WM_CTLCOLORSTATIC:', 'case WM_CTLCOLOREDIT:\n    case WM_CTLCOLORSTATIC:')
s=s.replace('px(540),px(290)', 'px(540),px(434)')
p.write_text(s)
