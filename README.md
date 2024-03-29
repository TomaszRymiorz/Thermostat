# Thermostat
Oprogramowanie termostatu automatycznego domu.

### Budowa termostatu
Termostat został zbudowany na bazie ESP8266 wraz z modułem RTC DS1307 oraz czujnikiem temperatury DS18B20. Uzupełnieniem jest 1 kanałowy przekaźnik SSR.

### Możliwości
Łączność z termostatem odbywa się przez sieć Wi-Fi.
Dane dostępowe do routera przechowywane są wraz z innymi informacjami w pamięci flash.
W przypadku braku informacji o sieci, urządzenie aktywuje wyszukiwania routera z wykorzystaniem funkcji WPS.

Termostat automatycznie łączy się z zaprogramowaną siecią Wi-Fi w przypadku utraty połączenia.

Zawiera czujnik temperatury wykorzystywany m.in. przez funkcje automatycznych ustawień. Dane z czujnika przesyłane są również do pozostałych urządzeń działających w systemie iDom będących w tej samej sieci Wi-Fi.

Urządzenie posiada opcję grzania do zadanej temperatury lub grzania przez określony czas, ustawienie przerwy w uruchamianiu ustawień automatycznych, możliwość kalibracji czujnika temperatury, ustawienie minimalnej temperatury oraz ustawienia wakacji od automatycznego grzania.

Zegar czasu rzeczywistego wykorzystywany jest przez funkcję ustawień automatycznych.
Ustawienia automatyczne obejmują minimalną temperaturę oraz możliwość ustawienia zakresu godzinowego.
Powtarzalność obejmuje okres jednego tygodnia, a ustawienia nie są ograniczone ilościowo. W celu zminimalizowania objętości wykorzystany został zapis tożsamy ze zmienną boolean, czyli dopiero wystąpienie znaku wskazuje na włączoną funkcję.

* 'o' poniedziałek, 'u' wtorek, 'e' środa, 'h' czwartek, 'r' piątek, 'a' sobota, 's' niedziela
* Brak wskazania dnia wygodnia oznacza, że ustawienie obejmuje cały tydzień
* 'n' wyzwalacz o zachodzie słońca.
* 'd' wyzwalacz o wschodzie słońca
* '<' wyzwalacz o zmroku
* '>' wyzwalacz o świcie
* 'z' wyzwalacz reaguj na zachmurzenie (po zmroku oraz po świcie)
* Każdy z powyższych wyzwalaczy może zawierać dodatkowe parametry zawarte w nawiasach, jak opóźnienie czasowe lub własne ustawienie LDR.
* 'l()', 'b()', 't()', 'c()' to wyzwalacze związane bezpośrednio z urządzeniem.
* 'l()' włączenie/wyłączenie światła
* 'b()', 'c()' pozycja rolety lub okna
* 't()' osiągnięcie określonej temperatury na termostacie
* '_' o godzinie - jeśli znak występuje w zapisie, przed nim znajduje się godzina w zapisie czasu uniksowego
* 'h(-1;-1)' między godzinami, jeśli obie cyfry są różne od "-1" lub po godzinie, przed godziną. "-1" oznacza, że nie ma wskazanej godziny
* '/' wyłącz ustawienie - obecność znaku wskazuje, że ustawienie będzie ignorowane
* '&' wszystkie wyzwalacze muszą zostać spełnione by wykonać akcje
* cyfra między symbolami "|" i "|" (lub "&" jako drugi symbol, jeśli jest wskazanie na wszystkie wyzwalacze) oznacza akcje do wykonania
* Obecność znaku 't' wskazuje, że ustawienie dotyczy termostatu.
* 'r()' i 'r2()' w nawiasach zawierają warunki, które muszą być spełnione w chwili aktywacji wyzwalacza, aby wykonać akcje
* 'r()' to wymaganie określonego stanu świateł, pozycji rolety, okna lub stanu czy temperatury termostatu
* 'r2()' wymaganie dotyczące pozycji słońca: wschód, zmierzch, świt, zmrok

### Sterowanie
Sterowanie urządzeniem odbywa się poprzez wykorzystanie metod dostępnych w protokole HTTP. Sterować można z przeglądarki lub dedykowanej aplikacji.

* "/hello" - Handshake wykorzystywany przez dedykowaną aplikację, służy do potwierdzenia tożsamości oraz przesłaniu wszystkich parametrów pracy urządzenia.

* "/set" - Pod ten adres przesyłane są ustawienia dla termostatu, dane przesyłane w formacie JSON. Ustawić można m.in. strefę czasową ("offset"), czas RTC ("time"), ustawienia automatyczne ("smart"), temperaturę lub czas grzania ("val"), dokonać kalibracji czujnika temperatury, jak również zmienić czas szybkiego dogrzania czy ustawić długość przerwy dla ustawień automatycznych.

* "/state" - Służy do regularnego odpytywania urządzenia o jego podstawowe stany, temperatura lub czas grzania i wskazania czujnika temperatury.

* "/basicdata" - Służy innym urządzeniom systemu iDom do samokontroli, urządzenia po uruchomieniu odpytują się wzajemnie o aktualny czas lub dane z czujników.

* "/log" - Pod tym adresem znajduje się dziennik aktywności urządzenia (domyślnie wyłączony).

* "/wifisettings" - Ten adres służy do usunięcia danych dostępowych do routera.
