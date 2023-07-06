# Утилита для компьютера. Передача файла по COM порту.

1. разобраться с передачей-приёмом данных по COM порту.
2. написать программы для передачи и приёма данных (это не обязательно должно быть одной программой)
3. реализовать некоторый простой протокол, для передачи файла.

В рамках протокола необходимо чтобы передающая сторона сообщала о:                                                        
3.10. открывала соединение                                                        
3.11. размере передаваемого файла
3.12. контрольную сумму CRC32 этого файла                                                        
3.13. запрашивала передачу данных                                                        
3.15. передавала фрагмент, с контрольной суммой фрагмента( CRC8 или 16 в зависимости от величины фрагмента).                                                  
3.16. получала ответ от принимающей стороны о успешной или неуспешной передачи фрагмента файла и после этого, если не весь файл был передан переходила к 
3.15. (если данные не были приняты корректно повторяем передачу этого же фрагмента. Тут следует сделать счётчик на максимальное количество передач одного фрагмента, и если количество некорректных передач одного пакета превысило определённое значение — закрыть соединение.)                                        
3.17. получала ответ от принимающей стороны о успешном принятии файла целиком.                                                        
3.18. закрывала соединение.                                                        

Принимающая сторона же должна сообщать о:                                                        
3.20 Открыть соединение                                                        
3.21 принять размер файла, сохранить его в памяти                                                        
3.22 принять контрольную сумму всего файла, сохранить её в памяти                                                        
3.23 дать ответ о готовности к приёму данных                                                        
3.24 принять фрагмент данных с контрольной суммой и посчитать, правильно ли передались данные                                                        
3.25 подсчитать все ли пакеты приняты, если не все, дать ответ о корректности или некорректности принятого фрагмента данных.  Если же файл принят целиком - подсчитать контрольную сумму файла и дать ответ о том что файл принят без ошибок                                                        
3.26 если количество ошибочных фрагментов одного пакета превысило определённое количество (счётчик) - закрыть соединение                                      
3.27 после получения всего файла и отправки сообщения о его принятии - закрыть соединение.