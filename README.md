# ESP8266_GasMeter
Arduino Code for an ESP-01S Module to get the Impulse Signals from a Gas Meter via MQTT to Home Assistant.

Here is the original version of the Code: https://www.kabza.de/MyHome/GasMeter/GasMeter.php

My version is heavily modified using ChatGPT.

The changes to the original version 2.12:
- GPIO Pins swapped because ESP-01S LED is on GPIO2
- WiFiManager added to better connect to WiFi Routers
- Webserver added
- Home Assistant autodiscovery feature added
- Value update through Home Assistant and Webserver
- Subscribe Topic changed to ESP8266_GasMeter/value/set
- Value is now also saved on device in case of power loss
- Nonblocking LED handling
- Switch automatically between german and english language
- MQTT data: Time, Ver, MAC, SSID, IP, Started and OpHrs is not send anymore
- Removed serial functionality
- ArduinoOTA Password: admin

## Deutsch:
Arduino Code für ein ESP-01S Modul um die Impulse Signale von einem Gaszähler via MQTT zu Home Assistant zu senden.

Hier ist die Original Version des Codes: https://www.kabza.de/MyHome/GasMeter/GasMeter.php

Meine Version wurde stark mittels ChatGPT verändert.

Die Änderungen zur Original Version 2.12:
- GPIO Pins vertauscht, da die auf dem ESP-01S Modul verbaute LED an GPIO2 angeschlossen ist
- WiFiManager hinzugefügt um sich besser mit dem WLAN Router verbinden zu können
- Webserver hinzugefügt
- Home Assistant Autodiscovery Funktion hinzugefügt um das Gerät selbständig finden zu lassen
- Zählerstandsanpassung mittels Home Assistant und Webserver ermöglicht
- Abonniertes MQTT Thema geändert zu ESP8266_GasMeter/value/set
- Der Zählerstandswert wird nun auch auf dem Gerät selbst gespeichert um nach einem Stromausfall direkt weiterzählen zu können
- Das blinken der LED führt nun nicht mehr dazu das die Ausführung des Programmcodes unterbrochen wird
- Automatischer Wechsel zwischen deutsch und englisch im Webserver, je nach Spracheinstellung im Browser/Betriebssystem
- Die MQTT Daten: Time, Ver, MAC, SSID, IP, Started und OpHrs werden nun nicht mehr gesendet
- Die serielle Schnittstelle wird nicht mehr benutzt
- Das ArduinoOTA Passwort für die Aktuallisierung des Programmcodes über das Netzwerk lautet: admin

### Anleitung:
- Den Code als .ino Datei herunterladen und in Arduino öffnen oder kompletten Code aus der .ino Datei kopieren und in die Arduino IDE einfügen.
- Ein ESP-01S Modul mit einem Programmieradapter an den Computer anschließen und den Port des Programmieradapters auswählen.
- Generic ESP8266 Module als Board auswählen. Wenn es das noch nicht gibt muss in den Einstellngen noch die URL hinzugefügt werden:
http://arduino.esp8266.com/stable/package_esp8266com_index.json und in der Board-Verwaltung ESP8266 installiert werden.
- Die fehlenden Bibliotheken hinzufügen: WiFiManager von tzapu und PubSubClient von Nick O'Leary
- Flash Size 4MB (FS:2MB OTA:1019KB) auswählen.
- Hochladen.
- Mit dem WiFi-AP: GasMeter-Setup verbinden.
- WLAN Router SSID zum Verbinden auswählen und WPA-Schlüssel eingeben.
- Die vom WLAN Router zugewiesene IP des Geräts in der Adresszeile des Browsers eingeben und Enter drücken.
- Auf MQTT konfigurieren klicken und den MQTT Broker wie z.B. homeassistant eingeben.
- Der Port ist standartmäßig 1883.
- Die MQTT Zugangsdaten des Brokers wie Benutzername (z.B. mqtt) und Passwort eingeben.
- Mit Klick auf Speichern bestätigen.
- Das ESP-01S Modul ist nun konfiguriert und kann mit dem Reed-Kontakt zwischen GPIO0 und GND verbunden werden.
- Der Reed-Kontakt ist sehr empfindlich und muss daher behutsam behandelt werden, damit er nicht kaputt geht.
- Das Biegen der Kontakte des Reed-Kontakts kann schon zur Zerstörung dessen führen. Auch sollte der Reed-Kontakt nicht zu stark gedrückt werden.
- Es ist keine Gute Idee zu versuchen den Reed-Kontakt an den Gaszähler zu kleben.
- Er wird sicher nicht in der korrkten Position bleiben und sich statt dessen wieder von selbst lösen.
- Beim festdrücken wird dieser höchstwahrscheinlich kaputt gehen.
- Die Kabel, die man an den Reed-Kontakt lötet oder steckt werden wahrscheinlich auch zu viel gewicht haben und das Bauteil zerstören.
- Meine Empfehlung ist daher ganz klar den Reed-Kontakt auf einer Platine zu befestigen und ab dann nur noch die Platine zu berühren und nicht mehr den Reed-Kontakt.
- Man kann dann statt des Reed-Kontakts einfach die Platine an den Gas-Zähler ankleben.
- Die Kabel bzw. die Kontakte zum ESP-01S Modul werden dann ebenfalls an die Platine angelötet.
- Wenn der Reed-Kontakt zusammen mit dem ESP-01 Modul am Gaszähler angebracht und mit 3,3 Volt Strom versorgt wird muss nur noch der Zählerstand des Gaszählers zum ESP-01 Modul geschickt werden, das geht entweder mit der Weboberfläche, per Home Assistant oder per MQTT Nachricht (z.B. 123.45) an ESP8266_GasMeter/value/set
- In dieser Version des Codes wurde der Check, ob der eingegebene Zählerstand höher ist als der bereits gespeicherte auskommentiert.
- Da in Home Assistant eingestellt ist, dass der Wert nur steigen darf und nie kleiner sein darf kann der Code bei Bedarf aber auch wieder aktiviert werden in dem man die // Zeichen im Code entfernt.
- Ich habe es auskommentiert, da ich den Fall hatte, dass der Impuls-Magnet bei mir einmal genau da stehen blieb wo der Reed-Kontakt ständig abwechselnd offen und gschlossen war und somit viel zu viele fehlerhafte Impulse an den Zähler gesendet hat. Daher musste ich den Wert nach unten korrigieren.

Getestet wurde es mit einem Honeywell BK-G4M Gaszähler.
