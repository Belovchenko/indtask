# Снимки файловой системы и дефрагментация
В данном индивидуальном задании курса "Конструирование ядра ОС" ставилась задача реализации фукционала снимков файловой системы (полных и инкрементальных) и дефрагментации диска в учебной операционной системе JOS.
Работа выполнялась в паре с моим товарищем и моей частью задания являлась реализация функций для работы инкрементальных снимков и дефрагментации диска.

Основной функционал описан в файле **fs/fs.c**, отвечающем за работу файловой системы.

В файле **fs/serv.c** описывается команда терминала "snapshot" и её флаги для вызова реализованных функций.
