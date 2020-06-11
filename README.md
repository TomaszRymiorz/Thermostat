# Thermostat
Oprogramowanie termostatu automatycznego domu.

### Budowa termostatu
Termostat został zbudowany na bazie ESP8266 wraz z modułem RTC DS1307 oraz czujnikiem temperatury DS18B20. Uzupełnieniem jest wyświetlacz OLED 128x64 SSD1306 oraz 1 kanałowy przekaźnik SSR.

### Możliwości
Łączność z termostatem odbywa się przez sieć Wi-Fi.
Dane dostępowe do routera przechowywane są wraz z innymi informacjami w pamięci flash.
W przypadku braku informacji o sieci, urządzenie aktywuje wyszukiwania routera z wykorzystaniem funkcji WPS.

Termostat automatycznie łączy się z zaprogramowaną siecią Wi-Fi w przypadku utraty połączenia.

Zawiera czujnik temperatury wykorzystywany przez funkcje automatycznych ustawień.

Urządzenie posiada opcję grzania do zadanej temperatury lub grzania przez określony czas, ustawienie przerwy w uruchamianiu ustawień automatycznych, możliwość kalibracji czujnika temperatury czy ustawienie minimalnej temperatury.

Zegar czasu rzeczywistego wykorzystywany jest przez funkcję ustawień automatycznych.
Ustawienia automatyczne obejmują minimalną temperaturę oraz możliwość ustawienia zakresu godzinowego.
Powtarzalność obejmuje okres jednego tygodnia, a ustawienia nie są ograniczone ilościowo. W celu zminimalizowania objętości wykorzystany został zapis tożsamy ze zmienną boolean, czyli dopiero wystąpienie znaku wskazuje na włączoną funkcję.

* wybrana minimalna temperatura umieszczona jest przed znacznikiem termostatu 't' i przed zdefiniowanymi dniami tygodnia
* 'w' cały tydzień
* 'o' poniedziałek, 'u' wtorek, 'e' środa, 'h' czwartek, 'r' piątek, 'a' sobota, 's' niedziela
* '_' od godziny - jeśli znak występuje w zapisie, przed nim znajduje się godzina w zapisie czasu uniksowego
* '-' do godziny - jeśli występuje w zapisie, po nim znajduje się godzina w zapisie czasu uniksowego
* '/' wyłącz ustawienie - obecność znaku wskazuje, że ustawienie będzie ignorowane

Przykład zapisu dwóch ustawień automatycznych: 360_20.5touehr-1260,/330_20.0tw-360

Obecność znaku 't' wskazuje, że ustawienie dotyczy termostatu.

### Sterowanie
Sterowanie termostatem odbywa się poprzez wykorzystanie metod dostępnych w protokole HTTP. Sterować można z przeglądarki lub dedykowanej aplikacji.

* "/hello" - Handshake wykorzystywany przez dedykowaną aplikację, służy do potwierdzenia tożsamości oraz przesłaniu wszystkich parametrów pracy termostatu.

* "/set" - Pod ten adres przesyłane są ustawienia dla termostatu, dane przesyłane w formacie JSON. Ustawić można m.in. strefę czasową ("offset"), czas RTC ("time"), ustawienia automatyczne ("smart"), temperaturę lub czas grzania ("val"), dokonać kalibracji czujnika temperatury, jak również zmienić czas szybkiego dogrzania czy ustawić długość przerwy dla ustawień automatycznych.

* "/state" - Służy do regularnego odpytywania termostatu o jego podstawowe stany, temperatura lub czas grzania i wskazania czujnika temperatury.

* "/basicdata" - Służy innym urządzeniom systemu iDom do samokontroli. Jeśli któreś urządzenie po uruchomieniu nie pamięta aktualnej godziny lub nie posiada czujnika światła, ta funkcja zwraca aktualną godzinę i dane z czujnika.

* "/log" - Pod tym adresem znajduje się dziennik aktywności urządzenia (domyślnie wyłączony).
